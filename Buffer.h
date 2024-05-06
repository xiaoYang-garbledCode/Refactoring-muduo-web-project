#pragma once

#include<string>
#include<vector>
/*
    成员变量：
        readerIndex_、writerIndex_
    prependable bytes |  readable bytes  | writable bytes   |
    0  <=         readerIndex   <=    writerIndex    <=    size
*/

class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        :buffer_(kCheapPrepend + kInitialSize)
        ,readerIndex_(kCheapPrepend)
        ,writerIndex_(kCheapPrepend)
    {}

    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    // 返回缓冲区可读数据的起始地址
    const char* peek() const
    {
        return begin() + readerIndex_;
    }
    // 读取缓冲区中 readerInedx_ => writeIndex_之间的可读数据
    void retrieve(size_t len)
    {
        if(len < readableBytes())
        {
            readerIndex_ +=len;
        }
        else
        {
            retrieveAll();
        }
    }
    // 读取缓冲区中所有的可读数据
    void retrieveAll()
    {
        // 因为此时没有可读数据了，将让readerIndex_与writerIndex_回到初始化的位置
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    // 把onMessage函数上报的Bufer数据转成string类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len); // 读完数据对缓冲区进行复位操作
        return result;
    }

    void ensureWriteableBytes(size_t len)
    {
        // 当前缓冲区不够写
        if(writableBytes() < len)
        {
            //扩容
            makeSpace(len);
        }
    }

    // 把[data, data+len]内存上的数据添加到writable缓冲区中
    void append(const char* data, size_t len)
    {
        // 先确保writable够len
        ensureWriteableBytes(len);
        // copy(begin, end, destination) 要拷贝的数据的[begin,end] -> 目的地的首地址
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }
    
    // Buffer可写的开始地址
    char* beginWrite() 
    {
        return begin() + writerIndex_;
    }

    // 从fd上读取数据
    ssize_t readFd(int fd, int* saveErrno);

    // 通过fd发送数据
    ssize_t writeFd(int fd, int* saveErrno);
private:
    // 返回vector底层数组首元素的地址，也是vector数组的起始地址
    char* begin()
    {
        return &*buffer_.begin();
    }
    const char* begin() const
    {
        return &*buffer_.begin();
    }
    void makeSpace(size_t len)
    {
        // len + kCheapPrepend(8)
        // prependableBytes 表示从Buffer起始到readerIndex_
        // writableBytes() + prependableBytes()代表除了可读数据外，Buffer的所有空间
        if(writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            // Buffer所有的空闲空间都不够
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readable = readableBytes();
            // 将可读部分的数据: readerIndex_ -> writerIndex_之间的数据
            // 移动到从kCheapPrepend开始的位置
            std::copy(begin() + readerIndex_, begin() + writerIndex_,
                            begin() + kCheapPrepend);
            // 可读部分的数据移动，readerIndex_也要改变
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};