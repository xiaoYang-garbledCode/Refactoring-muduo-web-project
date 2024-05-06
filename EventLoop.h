#pragma once
#include"noncopyable.h"
#include"CurrentThread.h"
#include"Timestamp.h"

#include<vector>
#include<atomic>
#include<memory>
#include<functional>
#include<mutex>
/*
    事件循环类， 主要包含Poller和Channel
    成员变量：
        标志当前的loop状态 looping、quit 都应该是线程安全的
        记录当前loop所在的线程id
        poller返回发生事件的channels的时间点
        指向poller的指针 
        mainLoop通过eventfd唤醒其他subLoop的变量 wakeupFd
        wakeupChannel
        存储Channel*的activeChannels
        标识当前loop是否需要执行回调操作 callingPendingFunctors
        存储loop需要执行的所有回调操作 pendingFunctors
        互斥锁，保证容器访问的线程安全
*/
class Poller;
class Channel;
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();
    // 返回poller发生事件的channels的时间点
    Timestamp pollReturnTime() const { return pollReturnTime_;}

    // 在当前的loop中执行cb
    void runInLoop(Functor cb);
    // 把cb放入队列中，唤醒loop所在的线程，执行cb
    void queueInLoop(Functor cb);
    
    // 唤醒loop所在的线程
    void wakeup();

    // channel调用loop的updateChannel，将相应的事件注册到poller中
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // 判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid();}
private:
    void handleRead();
    void doPendingFunctor();
    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;
    std::atomic_bool quit_;

    const pid_t threadId_;
    Timestamp pollReturnTime_;
    std::unique_ptr<Poller> poller_;

    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;
    ChannelList activeChannels_;

    std::atomic_bool callingPendingFunctors_;
    std::vector<Functor> pendingFunctors_;
    std::mutex mutex_;
};