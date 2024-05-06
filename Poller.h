#pragma once
#include<unordered_map>
#include<vector>

#include "noncopyable.h"
#include "Timestamp.h"
/*
    muduo库中多路事件分发器的核心, IO复用模块
    有一个ChannelList,代表这个poller观察的所有channel
    成员变量：
        ChannelMap: sockfd  sockfd所属的channel通道类型
        定义poller的事件循环eventLoop 一个loop对应一个poller
*/

class Channel;
class EventLoop;
class Poller : noncopyable
{
public:
    using ChannelLists = std::vector<Channel*>;
    Poller(EventLoop*);
    virtual ~Poller() = default;

    // 给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelLists *activeChannels) =0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    // 判断参数channel是否在当前Poller当中
    bool hasChannel(Channel* channel) const;

    // EventLoop可以通过该接口获取默认的IO复用的具体实现,如poll,epoll
    static Poller* newDefaultPoller(EventLoop *loop);
protected:
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;
private:
    EventLoop * ownerLoop_;
};