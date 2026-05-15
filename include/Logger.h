#pragma once

// 定义日志的级别 INFO ERROR FATAL DEBUG

#include "noncopyable.h"

#include <cstdio>
#include <mutex>
#include <string_view>

// LOG_INFO("%s %d", arg1, arg2)
#define LOG_INFO(logmsgFormat, ...)                               \
    do {                                                          \
        mymuduo::Logger& logger = mymuduo::Logger::getInstance(); \
        logger.setLogLevel(mymuduo::INFO);                        \
        char buf[1024]{};                                         \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);         \
        logger.log(buf);                                          \
    } while (0)

#define LOG_ERROR(logmsgFormat, ...)                              \
    do {                                                          \
        mymuduo::Logger& logger = mymuduo::Logger::getInstance(); \
        logger.setLogLevel(mymuduo::ERROR);                       \
        char buf[1024]{};                                         \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);         \
        logger.log(buf);                                          \
    } while (0)

#define LOG_FATAL(logmsgFormat, ...)                              \
    do {                                                          \
        mymuduo::Logger& logger = mymuduo::Logger::getInstance(); \
        logger.setLogLevel(mymuduo::FATAL);                       \
        char buf[1024]{};                                         \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);         \
        logger.log(buf);                                          \
        exit(-1);                                                 \
    } while (0)

#ifdef DEBUG_ON
#    define LOG_DEBUG(logmsgFormat, ...)                              \
        do {                                                          \
            mymuduo::Logger& logger = mymuduo::Logger::getInstance(); \
            logger.setLogLevel(mymuduo::DEBUG);                       \
            char buf[1024]{};                                         \
            snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);         \
            logger.log(buf);                                          \
        } while (0)
#else
#    define LOG_DEBUG(logmsgFormat, ...) \
        do {                             \
        } while (0)
#endif

namespace mymuduo {
enum LogLevel
{
    INFO,    // 普通信息
    ERROR,   // 错误信息
    FATAL,   // core 信息
    DEBUG    // 调试信息
};

// 日志类
class Logger : noncopyable
{
public:
    // 获取唯一日志实例对象
    static Logger& getInstance();

    // 设置日志级别
    void setLogLevel(int level);

    // 写日志
    void log(std::string_view msg) const;

private:
    int logLevel_;

    Logger() = default;
    ~Logger() = default;

    mutable std::mutex mutex_;
};
}   // namespace mymuduo