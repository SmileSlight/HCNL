#pragma once

/*
 * 用户使用muuduo编写服务器程序
*/
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

// 对外的服务器编程使用的类
class TcpServer : noncopyable
{
public:
        using ThreadInitCallback = std::function<void(EventLoop*)>;       

        enum Option
        {
                kNoReusePort,
                kReusePort,
        };

        TcpServer(EventLoop *loop,
                        const InetAddress &listenAddr,
                        const std::string &nameArg,
                        Option option = kNoReusePort);
        ~TcpServer();

        void setThreadInitCallback(const ThreadInitCallback &cb){ threadInitCallback_ = cb; }
        void setConnectionCallback(const ConnectionCallback &cb){ connectionCallback_ = cb; }
        void setMessageCallback(const MessageCallBack &cb){ messageCallBack_ = cb; }
        void setWriteCompleteCallback(const WriteCompleteCallback &cb){ writeCompleteCallback_ = cb; }
        void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark){ highWaterMarkCallback_ = cb; }
        
        // 设置底层subloop个数
        void setThreadNum(int numThreads);

        // 开启服务器监听
        void start();
private:
        void newConnection(int sockfd, const InetAddress &peerAddr);
        void removeConnection(const TcpConnectionPtr &conn);
        void removeConnectionInLoop(const TcpConnectionPtr &conn);

        using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

        EventLoop *loop_; // baseloop 用户定义的loop

        const std::string ipPort_; // 服务器的ip和端口
        const std::string name_; // 服务器名称

        std::unique_ptr<Acceptor> acceptor_; // 运行在mainloop上的acceptor， 任务就是监听新连接事件
        
        std::shared_ptr<EventLoopThreadPool> threadPool_; // 线程池 one loop per thread

        ConnectionCallback connectionCallback_; // 有新连接时的回调
        MessageCallBack messageCallBack_; // 有读写消息时的回调
        WriteCompleteCallback writeCompleteCallback_; // 写完成时的回调
        HighWaterMarkCallback highWaterMarkCallback_; // 高水位回调

        ThreadInitCallback threadInitCallback_; // 线程初始化时的回调

        std::atomic_int started_;

        int nextConnId_;
        ConnectionMap connections_; // 保存所有的连接
};