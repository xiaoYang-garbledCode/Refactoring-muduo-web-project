#pragma once
#include"Thread.h"

#include<condition_variable>
#include<functional>
#include<mutex>
/*
    将loop和thread结合在一起
    成员变量：
        *loop、exiting_
        Thread、mutex_、cond、callback
*/
class EventLoop;

class EventLoopThread
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *loop)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
        const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();
private:
    void threadfunc();

    EventLoop *loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};