#include<memory>
#include<functional>

#include"noncopyable.h"
#include"Timestamp.h"
/*
EventLoop  Poller  Channel 三者   
            =》Reactor模型上对应的Demultiplex
Channel理解为通道，封装sockfd与其感兴趣的event，如EPOLLIN,EPOLLOUT
待poller返回发送的revents

成员变量： 事件循环、fd 即poller监听的对象、注册fd感兴趣的事件
            poller返回具体发生的事件、代表channel是否添加到poller中的标志index
            防止当conn连接被删除时，channel底层仍然在执行相应的回调操作


*/
class EventLoop;

class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到poller通知后处理事件
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象,conn通过这个函数来设置相应的回调函数
    void setReadCallback(ReadEventCallback cb){ readCallback_ = std::move(cb);}
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb){ closeCallback_ = std::move(cb);}
    void setErrorCallback(EventCallback cb){ errorCallback_ = std::move(cb);}

    // 防止channel被remove的时候仍然在执行回调操作,用弱指针指向强指针
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_;}
    int events() const { return events_;}
    // 提供给poller使用，设置发生的感兴趣事件
    void set_events(int revt) { revents_ = revt;}

    // 设置fd相应的事件状态,将每一种感兴趣的事件看作一位，对某个事件感兴趣就将对应的位置为1
    // 对某件事不感兴趣就&=~event,将对应的位置为0
    void enableReading() { events_ |= kReadEvent; update();}
    void disableReading() { events_ &= ~kReadEvent; update();}
    void enableWriting() { events_ |= kWriteEvent; update();}
    void disableWriting() { events_ &= ~kWriteEvent; update();}
    void disableAll() { events_ = kNoneEvent; update();}

    // 返回fd当前的事件状态, 
    // 因为kevent三个变量只有一位是1,因此如何events对应位为0,那么&出来的结果就是0 false
    bool isNoneEvent() { return events_ == kNoneEvent;}
    bool isWriting() { return events_ & kWriteEvent;}
    bool isReading() { return events_ & kReadEvent;}

    int index() { return index_;}
    // 表示channel是否添加到poller当中的标志,初始状态为-1,表示未加入
    void set_index(int idx) { index_ = idx; }

    // one loop per thread, 返回当前channel位于的loop
    EventLoop* onwerLoop() { return loop_;}
    // 在当前的loop中移除相应的channel
    void remove();
private:
    // 将感兴趣的事件设置到event当中
    void update();
    // 执行相应的回调操作，Guard使用的目的是防止channel被remove之后仍然执行回调操作
    void handleEventWithGuard(Timestamp receiveTime);

    // EPOLLIN EPOLLPRI EPOLLOUT
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;
    EventLoop* loop_;
    const int fd_;
    int index_;
    int events_;
    int revents_;

    std::weak_ptr<void> tie_;
    bool tied_;

    // 因为Channel可以获知fd最终发生的具体事件
    // 所有它负责调用具体事件的回调操作  
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};