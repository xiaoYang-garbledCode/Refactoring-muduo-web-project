#include"EPollPoller.h"
#include"Logger.h"
#include"Channel.h"

#include<error.h>
#include<unistd.h>
#include<strings.h>
// channel的index, -1表示这个channel还没用添加到poller中
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;

 EPollPoller::EPollPoller(EventLoop *loop)
    :Poller(loop)
    ,epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    ,events_(kInitEventListSize)
 {
    if(epollfd_<0)
    {
        LOG_FATAL("epoll_create error%d \n", errno);
    }
 }
 
EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

    // 重写基类Poller的抽象方法
Timestamp EPollPoller::poll(int timeoutMs, ChannelLists* activeChannels_)
{
    // 实际上应该使用LOG_DEBUG来输出日志更为合理
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());
    int numEvents = ::epoll_wait(epollfd_,  &*events_.begin(), static_cast<int>(events_.size()),timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());
    if(numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels_);
        if(numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if(numEvents == 0)
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else
    {
        if(saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

void EPollPoller::updateChannel(Channel* channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(),
                                 channel->events(), index);
    if(index ==kNew || index == kDeleted)
    {
        if(index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else
    {
        // channel已经在poller上注册过了
        int fd = channel->fd();
        if(channel->isNoneEvent())
        {
            // 如果channel上没用感兴趣的事件，需要从poller上删除
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel* channel) 
{
    int fd = channel->fd();
    channels_.erase(fd);
    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    int index = channel->index();
    if(index == kAdded)
    {
        // 当前的通道在poller注册过，需要删除
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 填写活跃的连接, 其中numEvents是有epoll_wait返回的发生事件的通道数量
void EPollPoller::fillActiveChannels(int numEvents, ChannelLists *activeChannels) const
{
    for(int i=0; i<numEvents; i++)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_events(events_[i].events);
        // EventLoop拿到Poller给它返回的所有发生事件的channel列表
        activeChannels->push_back(channel);
    }
}

// 更新channel通道的revents_  epoll_ctl (add/mod/del)
void EPollPoller::update(int operation, Channel* channel)
{
    epoll_event event;
    bzero(&event, sizeof event);

    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;

    if(::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if(operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}