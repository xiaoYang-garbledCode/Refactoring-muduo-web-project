#pragma once
#include<sys/syscall.h>
#include<unistd.h>
/*
    获取当前线程的tid
*/


namespace CurrentThread
{
    // thread_local 虽然是全局变量，但是这个变量在每一个线程里都存储一份，独立属于每一个线程
    extern __thread int t_cachedTid;
    // 获取线程tid，是一个系统调用，需要从用户态切换到内核态，所以第一次访问后就
    // 将它存储起来，后面再访问的时候就不需要再切换了。
    void cacheTid();

    inline int tid()
    {
        // 语句优化，t_cachedTid是否获取过
        if(__builtin_expect(t_cachedTid == 0, 0))
        {
            cacheTid();
        }
        return t_cachedTid;
    }
}