#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/*
 * 从fd上读取数据 Poller工作在LT模式
 * Buffer缓冲区是有大小的！ 但是从fd上读数据的时候，却不知道Tcp数据最终的大小
*/
ssize_t Buffer::readFd(int fd, int *savedErrno)
{
        char extrabuf[65536] = { 0 }; // 栈上的内存空间 64k
        struct iovec vec[2];
        const size_t writable = writableBytes(); // 这是Buffer缓冲区剩余的可写空间大小
        vec[0].iov_base = begin() + writerIndex_; // Buffer缓冲区的可写起始地址
        vec[0].iov_len = writable; // Buffer缓冲区的可写空间大小

        vec[1].iov_base = extrabuf; // 栈上的内存空间
        vec[1].iov_len = sizeof(extrabuf); // 栈上的内存空间大小

        const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1; // 一次最多读取64k数据
        const ssize_t n = readv(fd, vec, iovcnt); // 从fd上读取数据
        if (n < 0)
        {
                *savedErrno = errno;
        }
        else if (static_cast<size_t>(n) <= writable) // 读取的数据小于等于Buffer缓冲区的可写空间大小
        {
                writerIndex_ += n; // 移动writerIndex_指针
        }
        else // 读取的数据大于Buffer缓冲区的可写空间大小
        {
                writerIndex_ = buffer_.size(); // 移动writerIndex_指针到缓冲区的末尾
                append(extrabuf, n - writable); // 把栈上的内存空间的数据，追加到Buffer缓冲区当中
        }

        return n;
}

ssize_t Buffer::writeFd(int fd, int *savedErrno)
{
        ssize_t n = ::write(fd, peek(), readableBytes()); // 从Buffer缓冲区中读取数据，发送到fd上
        if (n < 0)
        {
                *savedErrno = errno;
        }
        return n;
}