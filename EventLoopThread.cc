#include"EventLoopThread.h"
#include"EventLoop.h"
EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,const std::string &name)
        :loop_(nullptr)
        ,exiting_(false)
        ,thread_(std::bind(&EventLoopThread::threadfunc,this), name)
        ,mutex_()
        ,cond_()
        ,callback_(cb)
        {
        }
EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if(loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}
// 启动底层的新线程，并且返回新创建的属于这个新线程的loop
EventLoop* EventLoopThread::startLoop()
{
    // 启动底层的新线程
    thread_.start();
    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr)
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}
// 下面这个方法，是在单独的新线程里运行的
void EventLoopThread::threadfunc()
{
    // 创建loop与新线程对应
    EventLoop loop;
    if(callback_)
    {
        callback_(&loop);
    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    // 开启事件循环
    loop.loop();
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}