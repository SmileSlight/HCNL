#pragma once

#include <vector>
#include <functional>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;

// 时间循环类 主要包含两大模块 Channel Poller（epoll的抽象）
class EventLoop : noncopyable
{
public: 
        using Functor = std::function<void()>;

        EventLoop();
        ~EventLoop();

        // 开启事件循环
        void loop();
        // 退出事件寻
        void quit();

        Timestamp pollReturnTime() const { return pollReturnTime_; }

        // 在当前loop中执行cb
        void runInLoop(Functor cb);
        // 将cb放入队列中，唤醒loop所在线程，执行cb
        void queueInLoop(Functor cb);

        // 用来唤醒loop所在线程
        void wakeup();

        // EventLoop的方法  ==>  Poller的方法
        void updateChannel(Channel* channel);
        void removeChannel(Channel* channel);
        bool hasChannel(Channel* channel);

        // 判断EventLoop是否在当前线程中
        bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
        void handleRead(); // wake up
        void doPendingFunctors(); // 执行loop中的回调函数

        using ChannelList = std::vector<Channel*>;

        std::atomic_bool looping_; // 通过CAS实现的原子变量，用于标记是否在事件循环中
        std::atomic_bool quit_; // 标记是否退出事件循环
        
        const pid_t threadId_; // 事件循环所属的线程id
        
        Timestamp pollReturnTime_; // poller返回发生事件的channels的时间点
        std::unique_ptr<Poller> poller_; // 事件分发器

        int wakeupFd_; // 主要作用：当mainLoop获取一个新用户的channel时，通过轮询算法选择一个subloop，通过该成员唤醒subLoop处理channel
        std::unique_ptr<Channel> wakeupChannel_; // 用于唤醒subLoop的channel

        ChannelList activeChannels_; // 保存发生事件的channel

        std::atomic_bool callingPendingFunctors_; // 标记当前loop是否有需要执行的回调函数
        std::vector<Functor> pendingFunctors_; // 保存需要在loop中执行的回调函数
        std::mutex mutex_; // 互斥锁，用来保护上面vector容器的线程安全操作
};