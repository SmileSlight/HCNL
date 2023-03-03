#include "TcpConnection.h"
#include "Logger.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Socket.h"

#include <functional>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <string>

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
        if (loop == nullptr)
        {
                LOG_FATAL("%s:%s:%d TcpConnection loop is null ! \n", __FILE__, __FUNCTION__, __LINE__);
        }
        return loop;
}

TcpConnection::TcpConnection(EventLoop* loop,
                        const std::string& nameArg,
                        int sockfd,
                        const InetAddress& localAddr,
                        const InetAddress& peerAddr)
                : loop_(CheckLoopNotNull(loop))
                , name_(nameArg)
                , state_(kConnecting)
                , reading_(true)
                , socket_(new Socket(sockfd))
                , channel_(new Channel(loop, sockfd))
                , localAddr_(localAddr)
                , peerAddr_(peerAddr)
                , highWaterMark_(64*1024*1024) // 64M
{
        // 给channel设置回调函数，poller给channel通知感兴趣的事件发生了，channel就会回调相应的操作函数
        channel_->setReadCallback(
                std::bind(&TcpConnection::handleRead, this, std::placeholders::_1)
        );
        channel_->setWriteCallback(
                std::bind(&TcpConnection::handleWrite, this)
        );
        channel_->setCloseCallback(
                std::bind(&TcpConnection::handleClose, this)
        );
        channel_->setErrorCallback(
                std::bind(&TcpConnection::handleError, this)
        );

        LOG_INFO("TcpConnection::ctor[%s] at %p fd=%d\n", name_.c_str(), this, sockfd);
        socket_->setKeepAlive(true);
}


TcpConnection::~TcpConnection()
{
        LOG_INFO("TcpConnection::dtor[%s] at %p fd=%d state=%d\n",
                name_.c_str(), this, channel_->fd(), (int)state_);
}

// 发送数据
void TcpConnection::send(const std::string &buf)
{
        if (state_ == kConnected)
        {
                if (loop_->isInLoopThread())
                {
                        sendInLoop(buf.c_str(), buf.size());
                }
                else
                {
                        loop_->runInLoop(std::bind(
                                &TcpConnection::sendInLoop,
                                this,
                                buf.c_str(),
                                buf.size()
                        ));
                }
        }
}

/*
 * 发送数据 应用写的快 而内核发送数据慢 需要把待发送数据写入缓冲区 而且设置了水位回调
*/
void TcpConnection::sendInLoop(const void* message, size_t len)
{
        ssize_t nwrote = 0;
        size_t remaining = len;
        bool faultError = false;

        // 之前调用过该connnection的shutdown 不能进行发送了
        if (state_ == kDisconnected)
        {
                LOG_ERROR("disconnected, give up writing \n");
                return;
        }

        // 表示channel_第一次开始写数据 而且outputBuffer_中没有数据
        if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
        {
                nwrote = ::write(channel_->fd(), message, len);
                if (nwrote >= 0)
                {
                        // 既然数据一次性写完了，就不需要再注册写事件了 
                        remaining = len - nwrote;
                        if (remaining == 0 && writeCompleteCallback_)
                        {
                                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                        }
                }
                else
                {
                        nwrote = 0;
                        if (errno != EWOULDBLOCK)
                        {
                                LOG_ERROR("TcpConnection::sendInLoop");
                                if (errno == EPIPE || errno == ECONNRESET)
                                {
                                        faultError = true;
                                }
                        }
                }
        }

        // 说明数据没有写完，需要把剩余的数据写入缓冲区 给channel_
        // 注册epollout事件 poller发现tcp缓冲区有空间了，就会通知sock-channel_，channel_就会调用 writeCallback_回调方法
        // 也就是调用TcpConnection::handleWrite方法 把缓冲区中的数据全部发送完成
        if (!faultError && remaining > 0)
        {
                size_t oldLen = outputBuffer_.readableBytes();
                if (oldLen + remaining >= highWaterMark_
                        && oldLen < highWaterMark_
                        && highWaterMarkCallback_)
                {
                        loop_->queueInLoop(std::bind(
                                highWaterMarkCallback_, shared_from_this(), oldLen + remaining
                        ));
                }
                outputBuffer_.append(static_cast<const char*>(message) + nwrote, remaining);
                if (!channel_->isWriting())
                {
                        channel_->enableWriting();  // 注册channel的写事件 否则poller不会给channel通知epollout
                }
        }
}

// 关闭连接
void TcpConnection::shutdown()
{
        if (state_ == kConnected)
        {
                setState(kDisconnecting);
                loop_->runInLoop(
                        std::bind(&TcpConnection::shutdownInLoop, this)
                );
        }
}

void TcpConnection::shutdownInLoop()
{ 
        if (!channel_->isWriting()) // 说明outputBuffer中的数据已经全部发送完毕
        {
                socket_->shutdownWrite();
        }

}

// 连接建立
void TcpConnection::connectEstablished()
{
        setState(kConnected);
        channel_->tie(shared_from_this());
        channel_->enableReading(); // 向poller注册channel的epollin事件 

        // 新连接建立 执行回调
        connectionCallback_(shared_from_this());
}

// 连接销毁
void TcpConnection::connectDestroyed()
{
        if (state_ == kConnected)
        {
                setState(kDisconnected);
                channel_->disableAll(); // 把channel的所有感兴趣事件 从poller中del掉

                connectionCallback_(shared_from_this());
        }
        channel_->remove(); // 把channel从poller中删除掉
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
        int savedErrno = 0;
        ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
        if (n > 0)
        {
                // 已建立连接的用户，有可读事件发生了，调用用户传入的回调函数
                messageCallBack_(shared_from_this(), &inputBuffer_, receiveTime);
        }
        else if (n == 0)
        {
                handleClose();
        }
        else
        {
                errno = savedErrno;
                LOG_ERROR("TcpConnection::handleRead");
                handleError();
        }
}

void TcpConnection::handleWrite()
{
        if (channel_->isWriting())
        {
                int savedErrno = 0;
                ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
                if (n > 0)
                {
                        outputBuffer_.retrieve(n);
                        if (outputBuffer_.readableBytes() == 0)
                        {
                                channel_->disableWriting();
                                if (writeCompleteCallback_)
                                {
                                        // 唤醒loop_ 对应的thread，执行回调函数
                                        loop_->queueInLoop(
                                                std::bind(writeCompleteCallback_, shared_from_this())
                                        );
                                }
                                if (state_ == kDisconnecting)
                                {
                                        shutdownInLoop();
                                }
                        }
                }
                else
                {
                        LOG_ERROR("TcpConnection::handleWrite");
                }
        }
        else
        {
                LOG_ERROR("Connection fd = %d is down, no more writing \n", channel_->fd());
        } 
        
}

// poller => channel::closeCallback => TcpConnection::handleClose => TcpConnection::connectionCallback_ => TcpServer::closeCallback_
void TcpConnection::handleClose()
{
        LOG_INFO("TcpConnection::handleClose fd = %d state = %d \n", channel_->fd(), (int)state_);
        setState(kDisconnected);
        channel_->disableAll();

        TcpConnectionPtr connPtr(shared_from_this());
        connectionCallback_(connPtr); // 执行连接关闭的回调
        closeCallback_(connPtr); // 关闭连接的回调 执行的是TcpServer::removeConnection回调方法
}

void TcpConnection::handleError()
{
        int optval;
        socklen_t optlen = sizeof(optval);
        int err = 0;
        if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
        {
                err = errno;
        }
        else
        {
                err = optval;
        }
        LOG_ERROR("TcpConnection::handleError name = %s - SO_ERROR = %d \n", name_.c_str(), err);
}
