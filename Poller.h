#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心： IO复用模块
class Poller : noncopyable
{
public:
        using ChannelList = std::vector<Channel*>;

        Poller(EventLoop* loop);
        virtual ~Poller() = default;

        // 给所有的IO复用保留统一的接口
        virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
        virtual void updateChannel(Channel* channel) = 0;
        virtual void removeChannel(Channel* channel) = 0; 

        // 判断参数channel是否在当前Poller中
        bool hasChannel(Channel* channel) const;

        // EventLoop 可以通过该接口获取默认的IO复用的具体实现
        static Poller* newDefaultPoller(EventLoop* Loop);
protected:
        // map-key: sockfd
        // map-value: sockfd所属的channel通道类型
        using ChannelMap = std::unordered_map<int, Channel*>;   //无需排序 速度更快
        ChannelMap channels_;   
private:
        EventLoop* ownerLoop_; // 定义Poller所属的事件循环
};