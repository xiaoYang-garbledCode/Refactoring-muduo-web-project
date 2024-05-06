#pragma once
#include"Timestamp.h"
#include"InetAddress.h"
#include"Callbacks.h"
#include"Buffer.h"
#include"noncopyable.h"

#include<string>
#include<atomic>
#include<memory>

/*
TcpServer -> acceptor -> 有用户连接,拿到connfd -> TcpConnection给这个connfd设置回调 -> channel -> Poller
    -> channel回调操作

    成员变量：
        loop_ 但这里绝对不是baseLoop，因为TcpConnection都是在subLoop中管理的
        name_、state_、reading_、
        与Acceptor类似， 有socket和channel
        localAddr和peerAddr
*/
class EventLoop;
class Socket;
class Channel;
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop,
                std::string &name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_;}
    const InetAddress& localAddress() const { return localAddr_;}
    const InetAddress& peerAddress() const { return peerAddr_;}

    bool connected() const { return state_ == kConnected;}

    // 发送数据
    void send(const std::string &buf);
    // 关闭连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = std::move(cb);}
    void setMessageCallback(const MessageCallback& cb)
    { messageCallback_ = std::move(cb);}
    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = std::move(cb);}
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb)
    { highWaterMarkCallback_ = std::move(cb);}
    void setCloseCallback(const CloseCallback &cb)
    { closeCallback_ = std::move(cb);}
    
    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();
private:
    enum StateE {kDisconnected, kConnecting, kConnected, kDisconnecting};
    void setState(StateE state) { state_ = state;}

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();

    EventLoop *loop_;
    std::string name_;
    // 一共有四种状态，初始状态为kConnecting,因为mainLoop分配subLoop后，
    // 调用tcpConnection一定是正在连接的状态，因此初始状态为kConnecting
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_; //有新连接时的回调
    MessageCallback messageCallback_; // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_;// 接收数据的缓冲区
    Buffer outputBuffer_; // 发送数据的缓冲区
};