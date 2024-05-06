#include<sys/epoll.h>

#include"Channel.h"
#include"Logger.h"
#include"EventLoop.h"
// 类内的静态变量需要在类外进行初始化
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    :loop_(loop)
    ,fd_(fd)
    ,index_(-1)
    ,events_(0)
    ,revents_(0)
    ,tied_(false)
{}

Channel::~Channel()
{}
// 在当前的loop中移除相应的channel
void Channel::remove()
{
    loop_->removeChannel(this);
}
// 将感兴趣的事件设置到event当中, 即在poller当中的epoll_ctl,需要通过eventLoop来更新poller
void Channel::update()
{
    loop_->updateChannel(this);
}
// 执行相应的回调操作，Guard使用的目的是防止channel被remove之后仍然执行回调操作
void Channel::handleEvent(Timestamp receiveTime)
{
    if(tied_) // Channel已经在conn里established了
    {
        // 在执行任务的时候需要保证channel没用被remove, 
        // 弱智能指针可以升级为强智能指针说明该对象还存在
        std::shared_ptr<void> guard = tie_.lock();
        if(guard)
        {
            // 执行相应的回调操作
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        // ？？？
        handleEventWithGuard(receiveTime);
    }
}

// 执行相应的回调操作，Guard使用的目的是防止channel被remove之后仍然执行回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    // 根据revents_ 执行相应的cb
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if(closeCallback_)
        {
            closeCallback_();
        }
    }

    if(revents_ & EPOLLERR)
    {
        if(errorCallback_)
        {
            errorCallback_();
        }
    }

    if(revents_ & (EPOLLIN || EPOLLPRI))
    {
        if(readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if(revents_ & EPOLLOUT)
    {
        if(writeCallback_)
        {
            writeCallback_();
        }
    }
}

// 防止channel被remove的时候仍然在执行回调操作,用弱指针指向强指针
// tie操作什么时候被调用？在conn的connect established的时候使用
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}