#pragma once

#include"noncopyable.h"

#include<memory>
#include<thread>
#include<functional>
#include<string>
#include<atomic>
class Thread
{
public:
    using threadfunc = std::function<void()>;
    explicit Thread(threadfunc, const std::string &name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string& name() const { return name_; }

    static int numCreated() { return numCreated_; }
private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    threadfunc func_;
    std::string name_;
    static std::atomic_int numCreated_;
};