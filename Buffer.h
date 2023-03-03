#pragma once 

#include <vector>
#include <string>

// 网络库底层的缓冲区类型定义
class Buffer
{
public:
        static const size_t kCheapPrepend = 8;
        static const size_t kInitialSize = 1024;

        explicit Buffer(size_t initialSize = kInitialSize)
                : buffer_(kCheapPrepend + initialSize),
                readerIndex_(kCheapPrepend),
                writerIndex_(kCheapPrepend)
        {}

        size_t readableBytes() const { return writerIndex_ - readerIndex_; }
        size_t writableBytes() const { return buffer_.size() - writerIndex_; }
        size_t prependableBytes() const { return readerIndex_; }

        // 返回缓冲区中刻度数据的起始地址
        const char * peek() const { return begin() + readerIndex_; }

        // onMessage string <== Buffer
        void retrieve(size_t len)
        {
               if (len < readableBytes())
               {
                       readerIndex_ += len;  // 只读了len长度的数据，移动readerIndex_指针
               }
               else  // len == readableBytes()
               {
                       retrieveAll();
               }
        }

        void retrieveAll()
        {
                readerIndex_ = kCheapPrepend;
                writerIndex_ = kCheapPrepend;
        }

        // 把onMessage函数上报的Buffer数据，转成string类型的数据返回
        std::string retrieveAllAsString()
        {
                return retrieveAsString(readableBytes()); // 应用可读取数据的长度
        }

        std::string retrieveAsString(size_t len)
        {
                std::string result(peek(), len); // 从缓冲区中取出数据
                retrieve(len);  // 从缓冲区中移除数据 复位
                return result;
        }

        // buffer_.size() - writerIndex_ >= len
        void ensureWritableBytes(size_t len)
        {
                if (writableBytes() < len)
                {
                        makeSpace(len);  // 扩容
                }
        }

        // 把[data, data + len] 内存上的数据 添加到wirtable缓冲区当中
        void append(const char* data, size_t len)
        {
                ensureWritableBytes(len);
                std::copy(data, data + len, beginWrite());
                writerIndex_ += len;
        }

        char* beginWrite() { return begin() + writerIndex_; }

        const char* beginWrite() const { return begin() + writerIndex_; }

        // 从fd上读取数据
        ssize_t readFd(int fd, int *savedErrno);
        // 通过fd发送数据
        ssize_t writeFd(int fd, int *savedErrno);
private:
        char * begin() 
        {       
                // &(it.operator*()) 
                return &*buffer_.begin();  // vector 底层数组首元素的地址，也就是数组的起始地址
        }

        const char * begin() const
        {
                return &*buffer_.begin();
        }

        void makeSpace(size_t len)
        {
                /*
                kCheapPrepend | reader | writer |
                kCheapPrepend |           len         |
                */
                if (writableBytes() + prependableBytes() < len + kCheapPrepend)
                {
                        buffer_.resize(writerIndex_ + len);
                }
                else
                {
                        size_t readable = readableBytes();
                        std::copy(begin() + readerIndex_,
                                        begin() + writerIndex_,
                                        begin() + kCheapPrepend);
                        readerIndex_ = kCheapPrepend;
                        writerIndex_ = readerIndex_ + readable;
                }
        }

        std::vector<char> buffer_;
        size_t readerIndex_;
        size_t writerIndex_;
};