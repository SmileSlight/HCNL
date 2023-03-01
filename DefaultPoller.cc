#include "Poller.h"
#include <stdlib.h>

Poller* Poller::newDefaultPoller(EventLoop* Loop)
{
        if(::getenv("MUDUO_USE_POLL"))
        {
                return nullptr; //生成poll的实例
        }
        else
        {
                return nullptr; //默认生成epoll的实例
        }           
}