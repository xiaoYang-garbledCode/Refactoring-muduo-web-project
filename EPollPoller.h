#pragma once
#include"Poller.h"

#include<vector>
#include<sys/epoll.h>
/*
    epoll的使用  epoll_create  epoll_ctl (add/remove/del) epoll_wait
    成员变量:
        epollfd EventList(用来存储epooll_event)
*/
class EventLoop;

class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    // 重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelLists* activeChannels_) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;
private:
    // EventList的初始大小(epoll_event的初始数量)
    static const int kInitEventListSize = 16;
    // 填写活跃的连接, 其中numEvents是有epoll_wait返回的发生事件的通道数量
    void fillActiveChannels(int numEvents, ChannelLists *activeChannels) const;
    // 更新channel通道的revents_
    void update(int operation, Channel* channel);
    using EventList = std::vector<epoll_event>;
    int epollfd_;
    EventList events_;
};