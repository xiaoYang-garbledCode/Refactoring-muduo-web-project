#include"EventLoop.h"
#include"EPollPoller.h"
#include"Logger.h"
#include"Channel.h"

#include<sys/eventfd.h>
// 防止一个线程创建多个EventLoop   __thread 表示thread_local
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;
// 创建wakeupFd，用来notify唤醒subReactor处理新来的channel
int createEventFd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd<0)
    {
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    :looping_(false)
    ,quit_(false)
    ,callingPendingFunctors_(false)
    ,threadId_(CurrentThread::tid())
    ,poller_(EPollPoller::newDefaultPoller(this))
    ,wakeupFd_(createEventFd())
    ,wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if(t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n",
                    t_loopInThisThread, threadId_);          
    }
    else
    {
        t_loopInThisThread = this;
    }
    // 设置wakeupChannel感兴趣的事件类型(EPOLLIN)，以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();
}
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while(!quit_)
    {
        activeChannels_.clear();
        // 监听两类fd  一种是client的fd，一种是wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for(Channel *channel : activeChannels_)
        {
            channel->handleEvent(pollReturnTime_);
        }
        doPendingFunctor();
    }

    LOG_INFO("EventLoop %p stop looping \n", this);
    looping_ = false;
}

// 退出事件循环
void EventLoop::quit()
{
    quit_ = true;
    // 如果是在其他线程调用的quit，例如在一个subLoop(woker)中，调用了mainLoop(IO)的quit
    if(!isInLoopThread())
    {
        // 唤醒这个线程(此时是阻塞在poller_->poll)，让他结束loop.loop()
        wakeup();
    }
    // 如果在自己的线程中，将quit改为true即可
}
// 在当前的loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    //在当前的loop线程中执行cb
    if(isInLoopThread())
    {
        cb();
    }
    else
    {
        // 在非当前loop线程中执行cb，需要唤醒loop所在线程执行cb
        queueInLoop(cb);
    }
}
// 把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    // 唤醒相应的需要执行上述回调操作的loop线程
    // || callingPendingFunctors_表示 当前loop正在执行回调，但是loop又有了新的回调
    if(!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); // 唤醒对应的loop所在的线程
    }
}

// 唤醒loop所在的线程， 向wakeupfd_ 写一个数据，wakeupChannel就发生读事件，loop线程会被唤醒
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n",n);
    }
}

// channel调用loop的updateChannel，将相应的事件注册到poller中
void EventLoop::updateChannel(Channel* channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel* channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel* channel)
{
    return poller_->hasChannel(channel);
}
void EventLoop::handleRead()
{
    uint64_t one=1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu instead of 8 \n", n);
    }
}
// 执行回调
void EventLoop::doPendingFunctor()
{
    // functors的目的是接收当前的所有functor，这样可以更快施放pendingFunctors,提高效率
    std::vector<Functor> functors;
    callingPendingFunctors_ = true; 
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor &functor : functors)
    {
        functor();
    }
    callingPendingFunctors_ = false;
}