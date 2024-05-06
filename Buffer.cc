#include"Buffer.h"

#include<unistd.h>
#include<sys/uio.h>
#include<errno.h>
/*
    从fd上读取数据  Poller工作在LT模式 level trigger
    Buffer缓冲区是由大小的，但是从fd上读取数据的时候是不知道tcp数据最终的大小
*/

// 从fd上读取数据
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    // 栈上的内存空间 64K
    char extrabuf[65536] = {0};
    // iovec允许用户一次性从多个缓冲区读取数据到一个单独的连续缓冲区
    // 或者将连续缓冲区的数据写入多个分散的缓冲区
    struct iovec vec[2];
    
    // 缓冲区剩余的可写空间大小
    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2:1;
    const ssize_t n = readv(fd, vec, iovcnt);
    if(n<0)
    {
        *saveErrno = errno;
    }else if(n <= writable) // Buffer的可写缓冲区已经够存储读出来的数据了
    {
        writerIndex_ += n;
    }
    else
    {
        // extrabuf里面也写入数据了, 说明缓冲区的writable区域被写满了
        writerIndex_ = buffer_.size();
        // 将extrabuf的数据append到缓冲区中，此时会makespace()
        append(extrabuf, n - writable);
    }
    return n;
}

// 通过fd发送数据
ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), writableBytes());
    if(n<0)
    {
        *saveErrno = errno;
    }
    return n;
}