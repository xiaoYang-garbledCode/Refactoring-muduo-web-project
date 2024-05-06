#pragma once
// 这里需要所以其他头文件，这样用户在使用的时候只要使用这个头文件即可
#include"noncopyable.h"
#include"Callbacks.h"
#include"InetAddress.h"
#include"Buffer.h"
#include"TcpConnection.h"
#include"EventLoop.h"
#include"EventLoopThreadPool.h"
#include"Acceptor.h"

#include<memory>
#include<unordered_map>
#include<string>
#include<functional>
#include<atomic>
/*
    用户使用muduo编写服务器程序
    成员变量：
        connectionMap、eventLoop、ipPort、name_、Acceptor、threadPool、
        started、connections、
*/


// 对外服务器编程使用的类
class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop *loop,
                const InetAddress &listenAddr,
                const std::string &nameArg,
                Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitcallback(const ThreadInitCallback &cb)
    { threadInitCallback_ = std::move(cb);}
    void setConnectionCallback(const ConnectionCallback &cb)
    { connectionCallback_ = std::move(cb);}
    void setMessageCallback(const MessageCallback &cb)
    { messageCallback_ = std::move(cb);}
    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    { writeCompleteCallback_ = std::move(cb);}

    // 设置底层subloop的个数
    void setThreadNum(int numThreads);

    // 开启服务器监听
    void start();
private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop *loop_; // baseLoop 用户定义的loop

    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_; // 运行再mainLoop，任务就是监听新连接事件

    std::shared_ptr<EventLoopThreadPool> threadPool_; // one loop per thread

    ConnectionCallback connectionCallback_; // 有新连接时的回调
    MessageCallback messageCallback_;   // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调

    ThreadInitCallback threadInitCallback_; // loop线程初始化的回调

    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_; // 保存所有的连接
};