#pragma once

#include "net/LogStream.h"
#include "net/noncopyable.h"

#include <cstddef>
#include <cstdint>

#ifndef LOG_ENABLED
#    define LOG_ENABLED 1
#endif

#ifndef DEFAULT_ASYNC_LOG
#    define DEFAULT_ASYNC_LOG 0
#endif

namespace mymuduo {

class NullLogStream
{
public:
    template<typename T>
    NullLogStream& operator<<(T&&) noexcept
    {
        return *this;
    }
};

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
        OFF,
        NUM_LOG_LEVELS
    };

    using OutputFunc = void (*)(const char* msg, std::size_t len);
    using FlushFunc = void (*)();

    struct Config
    {
        LogLevel level = INFO;                   // 日志级别
        bool enabled = true;                     // 是否开启日志
        bool async = (DEFAULT_ASYNC_LOG != 0);   // 是否启用异步日志
        bool toStdout = true;                    // 是否输出到控制台
        std::string logFile;                     // 日志文件路径，为空则不输出到日志
        bool append = true;                      // 是否追加写入
        int flushInterval = 3;                   // flush 间隔，单位秒

        // 日志滚动大小，超过 roolSize 后自动切换到新文件，为0则不滚动
        std::size_t rollSize = 64UL * 1024 * 1024;

        // 异步日志 buffer 的最大个数，数量满了会丢弃日志，并增加 droppedLogs 计数
        // 每个 buffer 默认 4MB
        std::size_t maxBufferSize = 32;
    };

    Logger(const char* file, int line, LogLevel level);
    Logger(const char* file, int line, const char* func, LogLevel level);
    Logger(const char* file, int line, const char* func, LogLevel level, int savedErrno);

    // 析构时真正输出日志
    ~Logger();

    LogStream& stream() { return stream_; }

    static void init(const Config& config);
    static void shutdown();
    static void flush();

    static LogLevel logLevel();
    static void setLogLevel(LogLevel level);

    static bool enabled();
    static void enableLogging(bool on = true);
    static void disableLogging() { enableLogging(false); }

    static bool isLevelEnabled(LogLevel level);
    static std::uint64_t droppedLogs();
    static const char* levelName(LogLevel level);

private:
    void formatTime();
    void finish();

    LogStream stream_;
    LogLevel level_;
    const char* file_;
    const char* func_;
    int line_;
    int savedErrno_;
};

}   // namespace mymuduo

#if LOG_ENABLED
#    define LOG_STREAM(level)                        \
        if (!mymuduo::Logger::isLevelEnabled(level)) \
            ;                                        \
        else                                         \
            mymuduo::Logger(__FILE__, __LINE__, __func__, level).stream()

#    define LOG_TRACE LOG_STREAM(mymuduo::Logger::TRACE)
#    define LOG_INFO LOG_STREAM(mymuduo::Logger::INFO)
#    define LOG_DEBUG LOG_STREAM(mymuduo::Logger::DEBUG)
#    define LOG_WARN LOG_STREAM(mymuduo::Logger::WARN)
#    define LOG_ERROR LOG_STREAM(mymuduo::Logger::ERROR)
#    define LOG_FATAL mymuduo::Logger(__FILE__, __LINE__, __func__, mymuduo::Logger::FATAL).stream()

/*
 * 系统错误日志。
 *
 * LOG_SYSERR:
 *   ERROR 级别，自动附带 errno 和 strerror(errno)。
 *
 * LOG_SYSFATAL:
 *   FATAL 级别，自动附带 errno 和 strerror(errno)，最后 abort。
 */
#    define LOG_SYSERR mymuduo::Logger(__FILE__, __LINE__, __func__, mymuduo::Logger::ERROR, errno).stream()
#    define LOG_SYSFATAL mymuduo::Logger(__FILE__, __LINE__, __func__, mymuduo::Logger::FATAL, errno).stream()

#else
#    define LOG_TRACE \
        if (true)     \
            ;         \
        else          \
            mymuduo::NullLogStream()

#    define LOG_DEBUG LOG_TRACE
#    define LOG_INFO LOG_TRACE
#    define LOG_WARN LOG_TRACE
#    define LOG_ERROR LOG_TRACE
#    define LOG_SYSERR LOG_TRACE

/*
 * FATAL 仍然保留。
 */
#    define LOG_FATAL mymuduo::Logger(__FILE__, __LINE__, __func__, mymuduo::Logger::FATAL).stream()
#    define LOG_SYSFATAL mymuduo::Logger(__FILE__, __LINE__, __func__, mymuduo::Logger::FATAL, errno).stream()
#endif