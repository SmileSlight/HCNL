#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <functional>
#include <strings.h>

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
        if (loop == nullptr)
        {
                LOG_FATAL("%s:%s:%d mainloop is null ! \n", __FILE__, __FUNCTION__, __LINE__);
        }
        return loop;
}

TcpServer::TcpServer(EventLoop *loop,
                        const InetAddress &listenAddr,
                        const std::string &nameArg,
                        Option option )
                        : loop_(CheckLoopNotNull(loop))
                        , ipPort_(listenAddr.toIpPort())
                        , name_(nameArg)
                        , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
                        , threadPool_(new EventLoopThreadPool(loop, name_ ))
                        , connectionCallback_()
                        , messageCallBack_()
                        , nextConnId_(1)
                        , started_(0)
{
        // 当有新用户连接时，会调用TcpServer::newConnection函数
        acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, 
                std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
        LOG_INFO("TcpServer::~TcpServer [%s] destructing \n", name_.c_str());
        for (auto &item : connections_)
        {
                 // 防止对象直接被释放 且出了该函数后对象会被释放
                 // 这个局部的shared_ptr智能指针对象，出右括号，可以自动释放new出来的TcpConnection对象
                TcpConnectionPtr conn(item.second);
                item.second.reset();

                // 销毁对象 
                conn->getLoop()->runInLoop(
                        std::bind(&TcpConnection::connectDestroyed, conn)
                );
                conn.reset();
        }
}

// 设置底层subloop个数
void TcpServer::setThreadNum(int numThreads)
{
        threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听 loop.loop()
void TcpServer::start()
{
       if (started_++ == 0)  // 防止一个TcpServer被start多次
       {
                threadPool_->start(threadInitCallback_); // 启动底层的loop线程池
                loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
       } 
}

// 有一个新的客户端的连接，Acceptor会执行这个回调
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
        // 轮询算法，从线程池中选择一个subloop，来管理channel
        EventLoop *ioLoop = threadPool_->getNextLoop();
        char buf[64] = {0};
        snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
        ++nextConnId_;
        std::string connName = name_ + buf;
        LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
                name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

        // 通过sockfd获取本地地址和端口信息
        sockaddr_in local;
        bzero(&local, sizeof(local));
        socklen_t addrlen = sizeof(local);
        if (::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
        {
                LOG_ERROR("sockets::getLocalAddr");
        }

        InetAddress localAddr(local);

        // 根据连接成功的sockdfd 创建一个TcpConnection连接对象
        TcpConnectionPtr conn(new TcpConnection(
                                ioLoop, 
                                connName, 
                                sockfd, 
                                localAddr, 
                                peerAddr));

        connections_[connName] = conn;
        // 下面的回调都是用户设置给TcpServer => TcpConnection => Channel => Poller => notify channel调用回调
        conn->setConnectionCallback(connectionCallback_);
        conn->setMessageCallBack(messageCallBack_);
        conn->setWriteCompleteCallback(writeCompleteCallback_);

        //设置了如何关闭连接的回调
        conn->setCloseCallback(
                std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)
        );

        // 直接调用TcpConnection::connectEstablished
        ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
        loop_->runInLoop(
                std::bind(&TcpServer::removeConnectionInLoop, this, conn)
        );
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
        LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s \n",
                name_.c_str(), conn->name().c_str());

        connections_.erase(conn->name());
        EventLoop *ioLoop = conn->getLoop();
        ioLoop->queueInLoop(
                std::bind(&TcpConnection::connectDestroyed, conn)
        );
}
