#include"TcpConnection.h"
#include"Socket.h"
#include"Channel.h"
#include"Logger.h"
#include"EventLoop.h"

#include<functional>
 TcpConnection::TcpConnection(EventLoop* loop,
                std::string &name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr)
    :loop_(loop)
    ,name_(name)
    ,socket_(new Socket(sockfd))
    ,channel_(new Channel(loop, sockfd))
    ,state_(kConnecting)
    ,reading_(false)
    ,localAddr_(localAddr)
    ,peerAddr_(peerAddr)
    ,highWaterMark_(64 * 1024 * 1024) // 64M
{
    // 给channel_设置相应的回调函数
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead,this,
                                std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite,this));

    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));

    channel_->setErrorCallback(std::bind(&TcpConnection::handleClose,this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name.c_str(), sockfd);
    socket_->setKeepAlive(true);
}
TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d\n", name_.c_str(), channel_->fd(),
                                            (int)state_);
}

// 发送数据
void TcpConnection::send(const std::string &buf)
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size()
            ));
        }
    }
}
// 关闭连接
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}
// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    // 新连接建立，执行回调
    connectionCallback_(shared_from_this());
}
// 连接销毁
void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}
// 给channel的setReadCallback绑定的回调函数
void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if(n>0)
    {
        // 已建立连接的以后有可读事件发送了，调用用户传入的回调操作onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if(n==0)
    {
        /*
            muduo只有一种关闭连接的方式：被动关闭连接。即对方先关闭连接，本地read(2)返回0
            的时候触发关闭逻辑。
            TcpConnection只提供了shutdown，这么做是为了收发数据的完整性
            当server shutWrite的时候，可以保证client发送的正在路上的数据能够被接收到。
        */
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}
void TcpConnection::handleWrite()
{
    if(channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if(n>0)
        {
            outputBuffer_.retrieve(n);
            // 可发生的数据全部发送完毕
            if(outputBuffer_.readableBytes() == 0)
            {
                // 唤醒loop_对应的thread线程，执行回调
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_,shared_from_this())
                );
            }
            if(state_ == kDisconnecting)
            {
                shutdownInLoop();
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handlewrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
    }
}
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(),
                                        (int)state_);
    setState(kDisconnected);
    // 将这个channel的所有感兴趣事件全部清空
    channel_->disableAll();
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);
    // 调用TcpServer绑定的回调操作
    closeCallback_(connPtr);
}
void TcpConnection::handleError()
{
    // 检查套接字是否存在错误并获取具体的错误errno
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen)<0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(),err);
}

void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    // 之前调用过改connection的shutdown，就不能再进行发送了
    if(state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }

    // 表示channel_第一次开始写数据，而且缓冲区没有待发送数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = write(channel_->fd(), data, len);
        if(nwrote >=0)
        {
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_)
            {
                // 既然在这里数据全部发送完成，就不用再给channel设置epollout事件了
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_,shared_from_this())
                );
            }
        }
        else // nwrote < 0
        {
            nwrote =0;
            if(errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if(errno == EPIPE || errno == ECONNRESET) // SIGPIPE RESET
                {
                    faultError = true;
                }
            }
        }
    }
    /*
        说明当前这次write,并没有把数据全部发送出去,剩余的数据需要保存到缓冲区当中,然后给channel
        注册epollout事件,poller发现tcp的发送缓冲区有空间,会通知相应的sock-channel,调用writeCallback_
        回调方法,也就是调用TcpConnection::handlewrite方法,把发送缓冲区当中的数据全部发送完成
    */
    if(!faultError && remaining >0)
    {
        // 目前发送缓冲区剩余的待发送数据的长度
        size_t oldLen = outputBuffer_.readableBytes();
        if(oldLen + remaining >= highWaterMark_ 
            && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining)
            );
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if(!channel_->isWriting())
        {
            // 这里一定要注册channel的写事件，否则poller不会给channel通知epollout
            channel_->enableWriting();
        }
    }
}
void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting())
    {
        socket_->shutdownWrite();
    }
}