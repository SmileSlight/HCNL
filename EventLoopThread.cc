#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadFunc& cb = ThreadInitCallback(), 
                const std::string& name = std::string())
        : loop_(nullptr)
        , exiting_(false)
        , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
        , mutex_()
        , cond_()
        , callback_(cb)
{

}

EventLoopThread::~EventLoopThread()
{
        exiting_ = true;
        if (loop_ != nullptr)
        {
                loop_->quit();
                thread_.join();
        }
}

EventLoopThread::EventLoop* startLoop()
{
        thread_.start();  // 启动底层的新线程

        EventLoop* loop = nullptr;
        {
                std::unique_lock<std::mutex> lock(mutex_);
                while (loop_ == nullptr)
                {
                        cond_.wait(lock);
                }
                loop = loop_;
        }
        return loop;
}

// 这个函数是在新线程中执行的
void EventLoopThread::threadFunc()
{
        EventLoop loop; // 创建一个独立的EventLoop和上面的线程一一对应， one loop per thread

        if (callback_)
        {
                callback_(&loop);
        }

        {
                std::unique_lock<std::mutex> lock(mutex_);
                loop_ = &loop;
                cond_.notify_one();
        }

        loop.loop(); // EventLoop loop => Poller.poll
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = nullptr;
}