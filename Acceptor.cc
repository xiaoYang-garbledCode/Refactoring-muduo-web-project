#include"Acceptor.h"
#include"Logger.h"
#include"InetAddress.h"

#include<sys/types.h>
#include<sys/socket.h>
#include<error.h>
#include<functional>
#include<unistd.h>
static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,0);
    if(sockfd < 0)
    {
        LOG_FATAL("%s%s%d listen socket create err:%d\n", __FILE__, __FUNCTION__, __LINE__,errno);
    }
    return sockfd;
}

 Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    :loop_(loop)
    ,acceptSocket_(createNonblocking())
    ,acceptChannel_(loop, acceptSocket_.fd())
    ,listenning_(false)
 {
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);

    // 给acceptChannel绑定一个回调handleRead，当有新用户连接的时候执行这个回调操作
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead,this));
 }
 Acceptor::~Acceptor()
{
    // 将acceptorChannel感兴趣的事件删除
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

// 将对应的sockefd设置listen,像acceptChannel注册对读感兴趣的事件
void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}
// listenfd有事件发生，就是有新用户连接了，这是给相应的acceptChannel绑定的回调函数，需要执行
void Acceptor::handleRead()
{
    InetAddress peeraddr;
    int connfd = acceptSocket_.accept(&peeraddr);
    if(connfd >=0)
    {
        if(newConnectionCallback_)
        {
            // 轮询找到subLoop,唤醒分发当前的新客户端的Channel
            newConnectionCallback_(connfd, peeraddr); 
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d \n",__FILE__, __FUNCTION__, __LINE__,errno);
        if(errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit! \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}