#pragma once

#include "noncopyable.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string_view>

// printf-style logging macros.
// Example: LOG_INFO("fd=%d peer=%s", fd, peer.c_str());
#define LOG_INFO(...)                                                                             \
    do {                                                                                          \
        char buf[1024];                                                                           \
        std::snprintf(buf, sizeof(buf), __VA_ARGS__);                                             \
        ::mymuduo::Logger::getInstance().log(::mymuduo::LogLevel::INFO, __FILE__, __func__, buf); \
    } while (0)

#define LOG_ERROR(...)                                                                             \
    do {                                                                                           \
        char buf[1024];                                                                            \
        std::snprintf(buf, sizeof(buf), __VA_ARGS__);                                              \
        ::mymuduo::Logger::getInstance().log(::mymuduo::LogLevel::ERROR, __FILE__, __func__, buf); \
    } while (0)

#define LOG_FATAL(...)                                                                             \
    do {                                                                                           \
        char buf[1024];                                                                            \
        std::snprintf(buf, sizeof(buf), __VA_ARGS__);                                              \
        ::mymuduo::Logger::getInstance().log(::mymuduo::LogLevel::FATAL, __FILE__, __func__, buf); \
        std::abort();                                                                              \
    } while (0)

#ifdef DEBUG_ON
#    define LOG_DEBUG(...)                                                                             \
        do {                                                                                           \
            char buf[1024];                                                                            \
            std::snprintf(buf, sizeof(buf), __VA_ARGS__);                                              \
            ::mymuduo::Logger::getInstance().log(::mymuduo::LogLevel::DEBUG, __FILE__, __func__, buf); \
        } while (0)
#else
#    define LOG_DEBUG(...) \
        do {               \
        } while (0)
#endif

namespace mymuduo {

enum class LogLevel
{
    DEBUG,
    INFO,
    ERROR,
    FATAL
};

class Logger : noncopyable
{
public:
    static Logger& getInstance();

    void log(LogLevel level, const char* file, const char* func, std::string_view msg) const;

private:
    Logger() = default;
    ~Logger() = default;

    static const char* levelName(LogLevel level);
    static const char* basename(const char* path);
    static std::string_view trimTrailingNewline(std::string_view msg);

    mutable std::mutex mutex_;
};

}   // namespace mymuduo
