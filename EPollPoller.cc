#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <strings.h>

// channel 未添加到poller中
const int kNew = -1;  // channel的成员初始化index_ = -1  
// channel 已经添加到poller中
const int kAdded = 1;
// channel 已经从poller中删除
const int kDeleted = 2;

EPollPooller::EPollPooller(EventLoop* Loop)
        : Poller(Loop), 
        epollfd_(::epoll_create1(EPOLL_CLOEXEC)), 
        events_(kInitEventListSize) // vector<epoll_event>
{       
        if (epollfd_ < 0)
        {
                LOG_FATAL("epoll_create1 error: %d", errno);
        }
}

EPollPooller::~EPollPooller()
{
        ::close(epollfd_);
}

Timestamp EPollPooller::poll(int timeoutMs, ChannelList* activeChannels) 
{       
        // 实际上应该用LOG_DEBUG输出日志更为合理 频繁使用
        LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

        int numEvents = ::epoll_wait(epollfd_, &*events.begin(), static_cast<int>(events_.size()), timeoutMs);
        int saveErrno = errno;
        Timestamp now(Timestamp::now());

        if (numEvents > 0)
        {
                LOG_INFO("%d events happened \n", numEvents);
                fillActiveChannels(numEvents, activeChannels);
                if (numEvents == events_.size())
                {
                        events_.resize(events_.size() * 2);
                }
        }
        else if (numEvents == 0)
        {
                LOG_DEBUG("%s timeout! \n", __FUNCTION__);
        }
        else 
        {
                if (saveErrno != EINTR)
                {
                        errno = saveErrno;
                        LOG_ERROR("EPollPoller::poll() err!");
                }
        }
        return now;
}

// channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel => EPollPoller updateChannel removeChannel
/*
 *              EventLoop   =>  poller.poll
 *             /       \
 *        CHannel     Poller
 *                      ChannelMap <fd, channel*>   epollfd
*/
void EPollPooller::updateChannel(Channel* channel)
{
        const int index = channel->index();
        LOG_INFO("func=%s => fd = %d events = %d index = %d\n",  __FUNCTION__, channel->fd(), channel->events(), index);

        if (index == kNew || index == kDeleted)
        {
                // a new one, add with EPOLL_CTL_ADD
                int fd = channel->fd();
                if (index == kNew)
                {
                        // channelMap_ <fd, channel*>
                        channels_[fd] = channel;
                }
                else // index == kDeleted
                {
                        // channelMap_ <fd, channel*>
                        channels_[fd] = channel;
                }

                channel->set_index(kAdded);
                update(EPOLL_CTL_ADD, channel); 
        }
        else 
        {
                // update existing one with EPOLL_CTL_MOD/DEL
                int fd = channel->fd();
                if (channel->isNoneEvent())
                {
                        update(EPOLL_CTL_DEL, channel);
                        channel->set_index(kDeleted);
                }
                else
                {
                        update(EPOLL_CTL_MOD, channel);
                }
        }
}

// 从poller中删除channel
void EPollPooller::removeChannel(Channel* channel) 
{
        int fd = channel->fd();
        channels_.erase(fd);

        LOG_INFO("func=%s => fd = %d\n", __FUNCTION__, fd);
        
        int index = channel->index();
        if (index == kAdded)
        {
                update(EPOLL_CTL_DEL, channel);
        }
        channel->set_index(kNew);
}

// 填写活跃的连接
void EPollPooller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
        for (int i = 0 ; i < numEvents ; ++i)
        {
           Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
           channel -> set_revents(events_[i].events);
           activeChannels->push_back(channel); 
           // EventLoop就拿到了它的poller给他返回的所有发生事件的channel列表了  
        }
}

// 更新channel通道 epoll_ctl add/mod/del
void EPollPooller::update(int operation, Channel* channel)
{
        struct epoll_event event;
        bzero(&event, sizeof(event));
        int fd = channel->fd();

        event.events = channel->events();
        event.data.fd = fd;
        event.data.ptr = channel;
        

        if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
        {
                if (operation == EPOLL_CTL_DEL)
                {
                        LOG_ERROR("epoll_ctl del error:%d\n", errno);
                }
                else
                {
                        LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
                }
        }
}