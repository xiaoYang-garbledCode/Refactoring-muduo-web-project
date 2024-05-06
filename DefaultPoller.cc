#include"Poller.h"
#include"EPollPoller.h"

#include<stdlib.h>

Poller* Poller::newDefaultPoller(EventLoop *loop)
{
    if(::getenv("MUDUO_USE_POLL"))
    {
        return nullptr; //需要生成poll实例，暂时不实现
    }
    else
    {
        return new EPollPoller(loop);
    }
}