#pragma once
#include"noncopyable.h"
#include"Socket.h"
#include"Channel.h"

#include<functional>
/*
    成员变量：
        loop_ Acceptor用的是用户定义的那个baseLoop
        Channel 这里定义的是acceptchannel
*/
class EventLoop;
class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = std::move(cb);
    }

    bool listening() { return listenning_;}
    void listen();
private:
    void handleRead();

    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};