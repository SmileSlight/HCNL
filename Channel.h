#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

class EventLoop;

/**
 * EventLoop 包含：
 * ①多个Channel（放入Channellist） 
 * ②一个Poller （Poller并不拥有Channel，只是持有Channel的裸指针，通过ChannelMap） 《= Reactor 模型上对应 Demultiplex
 * Channel 理解为通道 封装了sockfd和其感兴趣的event，如EPOLLIN、EPOLLOUT事件
 * 还绑定了poller返回的具体事件
*/
class Channel : noncopyable
{
public:
        using EventCallback = std::function<void()>;
        using ReadEventCallback = std::function<void(Timestamp)>;

        Channel(EventLoop* loop, int fd);
        ~Channel();

        // fd得到poller通知以后，处理事件
        void handleEvent(Timestamp receiveTime);

        // 设置回调函数对象
        void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
        void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
        void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
        void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

        // 防止当channel被手动remove掉后，channel还在执行回调操作
        void tie(const std::shared_ptr<void>&);

        int fd() const { return fd_; }
        int events() const { return events_; }
        int set_revents(int revt) { revents_ = revt; }

        // 设置fd相应的事件状态
        void enableReading() { events_ |= kReadEvent; update(); }
        void disableReading() { events_ &= ~kReadEvent; update(); }
        void enableWriting() { events_ |= kWriteEvent; update(); }
        void disableWriting() { events_ &= ~kWriteEvent; update(); }
        void disableAll() { events_ = kNoneEvent; update(); }

        // 返回fd当前的事件状态
        bool isNoneEvent() const { return events_ == kNoneEvent; }
        bool isWriting() const { return events_ & kWriteEvent; }
        bool isReading() const { return events_ & kReadEvent; }

        int index() { return index_; }
        void set_index(int idx) { index_ = idx; }

        // one loop per thread
        EventLoop* ownerLoop() { return loop_; } // 返回channel所属的loop
        void remove();
private:

        void update();
        void handleEventWithGuard(Timestamp receiveTime);

        static const int kNoneEvent;
        static const int kReadEvent;
        static const int kWriteEvent;

        EventLoop *loop_;      // 事件循环
        const int fd_;         // fd, Poller监听的对象
        int events_;           // 注册fd感兴趣的事件
        int revents_;          // Poller返回的具体发生事件
        int index_;            // 在Poller中的索引

        std::weak_ptr<void> tie_;
        bool tied_;

        // 因为channel通道里面能够获知fd最终发生的事件，所以可以根据fd发生的事件来调用不同的回调函数
        ReadEventCallback readCallback_;
        EventCallback writeCallback_;
        EventCallback closeCallback_;
        EventCallback errorCallback_;
};