#pragma once

#include "EventLoopThread.h"
#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;

class EventLoopThreadPool : noncopyable
{
public:
        using ThreadInitCallback = std::function<void(EventLoop*)>;

        EventLoopThreadPool(EventLoop *baseLoop, const std::string& nameArg);
        ~EventLoopThreadPool();

        void setThreadNum(int numThreads) { numThreads_ = numThreads; }

        void start(const ThreadInitCallback& cb = ThreadInitCallback());

        // 如果工作在多线程中， baseLoop会默认以轮询的方式分配channel给subloop
        EventLoop *getNextLoop();

        std::vector<EventLoop*> getAllLoops();

        bool started() const { return started_; }
        const std::string& name() const { return name_; }
private:

        EventLoop *baseLoop_; // EventLoop loop
        std::string name_;
        bool started_;
        int numThreads_;
        int next_;
        std::vector<std::unique_ptr<EventLoopThread>> threads_; // 包含了所有线程
        std::vector<EventLoop*> loops_; // 包含了线程循环的指针
};