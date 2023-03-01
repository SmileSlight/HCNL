#include "Poller.h"
#include "Channel.h"


Poller::Poller(EventLoop* loop)
        : ownerLoop_(loop)
{
}

bool Poller::hasChannel(Channel* channel) const
{
        auto it = channel_.find(channel->fd());
        return it != channel_.end() && it->second == channel; 
}
