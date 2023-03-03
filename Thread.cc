#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
        : started_(false)
        , joined_(false)
        , tid_(0)
        , func_(std::move(func))
        , name_(name)
{
        setDefaultName();
} 

Thread::~Thread()
{
        if (started_ && !joined_)
        {
                thread_->detach();  // thread体
        }
}

void Thread::start()  // 一个Thread对象 就是记录一个线程的详细信息
{
        started_ = true;
        sem_t sem;
        sem_init(&sem, false, 0);
        // 开启线程
        thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
            // 获取线程的tid值
            tid_ = CurrentThread::tid();
            sem_post(&sem);
            // 开启一个新线程，专门执行该线程函数
            func_(); 
        }));

        // 这里必须等待获取上面最新创建的线程的tid值
        sem_wait(&sem);
}

void Thread::join()
{
        joined_ = true;
        thread_->join();
}

void Thread::setDefaultName()
{
       int num = ++numCreated_; 
       if (name_.empty())
       {
                char buf[32] = {0};
                snprintf(buf, sizeof(buf), "Thread%d", num);
                name_ = buf;
       }
}