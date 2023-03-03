#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}

Channel::~Channel()
{
}

// channel 的 tie 方法什么时候调用过? 一个TcpConnection新连接创建的时候 TcpConnection => Channel 
void Channel::tie(const std::shared_ptr<void>& obj)
{
        tie_ = obj;
        tied_ = true;
}

/*
 *  当改变channel所表示fd的events事件后，update负责在poller里面更改fd相应的事件epoll_ctl
 *  EventLoop => ChannelList => Poller => epoll_ctl
*/
void Channel::update()
{
        // 通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
        loop_->updateChannel(this);
}

// 在channel所属的EventLoop中，把当前的channel删除掉
void Channel::remove()
{
        loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
        // 如果channel被tied，那么就把tied_设置为false，然后把tied_所指向的对象赋值给guard
        // 这样就保证了tied_所指向的对象的生命周期至少和channel一样长
        std::shared_ptr<void> guard;
        if (tied_)
        {
                guard = tie_.lock();
                if (guard)
                {
                        handleEventWithGuard(receiveTime);
                }
        }
        else
        {
                handleEventWithGuard(receiveTime);
        }
}

// 根据poller通知的channel发生的事件，调用相应的回调函数 
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
        LOG_INFO("channel handleEvent revents:%d", revents_);

        if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
        {
                if (closeCallback_) 
                {
                        closeCallback_();
                }
        }
        if (revents_ & EPOLLERR)
        {
                if (errorCallback_) 
                {
                        errorCallback_();
                }
        }
        if (revents_ & (EPOLLIN | EPOLLPRI))
        {
                if (readCallback_) 
                {
                        readCallback_(receiveTime);
                }
        }
        if (revents_ & EPOLLOUT)
        {
                if (writeCallback_) 
                {
                        writeCallback_();
                }
        }
}