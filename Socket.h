#pragma once

#include "noncopyable.h" 

class InetAddress;

// 封装socket fd
class Socket : noncopyable
{
public:
        explicit Socket(int sockfd)
                : sockfd_(sockfd)
        { }

        ~Socket();

        int fd() const { return sockfd_; }

        // 绑定地址
        void bindAddress(const InetAddress& localaddr);

        // 开始监听
        void listen();

        // 接受连接
        int accept(InetAddress* peeraddr);

        // 关闭写端
        void shutdownWrite();

        // 设置TCP连接选项
        void setTcpNoDelay(bool on);

        // 设置TCP连接选项
        void setReuseAddr(bool on);

        // 设置TCP连接选项
        void setReusePort(bool on);

        // 设置TCP连接选项
        void setKeepAlive(bool on);
        
private:
        const int sockfd_;
};