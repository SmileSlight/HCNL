#pragma once

#include "Poller.h"

#include <vector>
#include <sys/epoll.h>

class Channel;

/*
 * epoll的使用
 * epoll_create() 创建一个epoll句柄，返回值是epoll的句柄fd
 * epoll_ctl()  add/mod/del 控制epoll的行为，注册事件，修改事件，删除事件
 * epoll_wait() 等待事件的产生，类似于select()调用
*/
class EPollPooller : public Poller
{
public:
        EPollPooller(EventLoop* Loop);
        ~EPollPooller() override;

        // 重写基类Poller的抽象方法
        Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
        void updateChannel(Channel* channel) override;
        void removeChannel(Channel* channel) override;
private:
        static const int kInitEventListSize = 16;

        // 填写活跃的连接
        void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
        // 更新channel通道
        void update(int operation, Channel* channel);

        using EventList = std::vector<epoll_event>;

        int epollfd_;
        EventList events_;
};