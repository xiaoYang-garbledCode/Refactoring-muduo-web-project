#pragma once
#include<string>

#include"noncopyable.h"
/*
用#define来定义日志函数 LOG_INFO("%s %d", arg1, arg2)

定义日志的级别 INFO ERROR FATAL DEBUG

输出一个日志类
    获取日志唯一的实例对象

    设置日志级别

    写日志 [级别信息] time : msg
成员变量：日志级别
*/
#define LOG_INFO(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(INFO); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0) 

#define LOG_ERROR(logmsgFormat, ...) \
    do \
    { \
       Logger &logger = Logger::getInstance(); \
       logger.setLogLevel(ERROR); \
       char buf[1024] = {0}; \
       snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
       logger.log(buf); \
    }while(0)

#define LOG_FATAL(logmsgFormat, ...) \
    do \
    { \
       Logger &logger = Logger::getInstance(); \
       logger.setLogLevel(FATAL); \
       char buf[1024] = {0}; \
       snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
       logger.log(buf); \
       exit(-1); \
    }while(0)

#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...) \
    do \
    { \
       Logger &logger = Logger::getInstance(); \
       logger.setLogLevel(DEBUG); \
       char buf[1024] = {0}; \
       snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
       logger.log(buf); \
    }while(0)
#else
    #define LOG_DEBUG(logmsgFormat, ...)
#endif
enum LogLevel
{   
    INFO,
    ERROR,
    FATAL,
    DEBUG
};

class Logger : noncopyable
{
public:
    static Logger& getInstance();

    void setLogLevel(int level);

    void log(std::string msg);
private:
    int logLevel_;
};
