#include"Thread.h"
#include"CurrentThread.h"

#include<semaphore.h>

std::atomic_int Thread::numCreated_ = 0;

Thread::Thread(threadfunc func, const std::string &name)
    :func_(std::move(func))
    ,name_(name)
    ,tid_(0)
    ,started_(false)
    ,joined_(false)
{
    setDefaultName();
}
Thread::~Thread()
{
    // 设置为分离线程
    if(started_ && !joined_)
    {
        thread_->detach();
    }
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::start()
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, 0, 0);

    // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        tid_ = CurrentThread::tid();
        // 获取id post sem
        sem_post(&sem);
        // 这个线程执行的线程函数
        func_();
    }));

    // 这里必须等待上面创建的线程获得tid_
    sem_wait(&sem);
}
void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if(name_.empty())
    {
        char buf[64] = {0};
        snprintf(buf,sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}