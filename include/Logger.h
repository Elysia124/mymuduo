#pragma once

#include "LogStream.h"
#include "noncopyable.h"

#include <cstddef>
#include <mutex>

namespace mymuduo {

class Logger : noncopyable
{
public:
    enum LogLevel
    {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        NUM_LOG_LEVELS
    };

    using OutputFunc = void (*)(const char* msg, size_t len);
    using FlushFunc = void (*)();

    Logger(const char* file, int line, LogLevel level);
    ~Logger();

    LogStream& stream() { return stream_; }

    static LogLevel logLevel();
    static void setLogLevel(LogLevel level);

    static void setOutput(OutputFunc output);
    static void setFlush(FlushFunc flush);

private:
    void formatTime();
    void finish();

    LogStream stream_;
    LogLevel level_;
    const char* file_;
    int line_;

    std::mutex mutex_;
};

}   // namespace mymuduo

#define LOG_TRACE                                             \
    if (mymuduo::Logger::logLevel() > mymuduo::Logger::TRACE) \
        ;                                                     \
    else                                                      \
        mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::TRACE).stream()

#define LOG_DEBUG                                             \
    if (mymuduo::Logger::logLevel() > mymuduo::Logger::DEBUG) \
        ;                                                     \
    else                                                      \
        mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::DEBUG).stream()

#define LOG_INFO                                             \
    if (mymuduo::Logger::logLevel() > mymuduo::Logger::INFO) \
        ;                                                    \
    else                                                     \
        mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::INFO).stream()

#define LOG_WARN                                             \
    if (mymuduo::Logger::logLevel() > mymuduo::Logger::WARN) \
        ;                                                    \
    else                                                     \
        mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::WARN).stream()

#define LOG_ERROR                                             \
    if (mymuduo::Logger::logLevel() > mymuduo::Logger::ERROR) \
        ;                                                     \
    else                                                      \
        mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::ERROR).stream()

#define LOG_FATAL mymuduo::Logger(__FILE__, __LINE__, mymuduo::Logger::FATAL).stream()
