#include "net/Logger.h"
#include "net/AsyncLogging.h"
#include "net/LogFile.h"
#include "net/Timestamp.h"

#include "net/CurrentThread.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace mymuduo {

namespace {

const char* kLogLevelName[Logger::NUM_LOG_LEVELS] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF"};

std::atomic<Logger::LogLevel> g_logLevel{Logger::INFO};

std::atomic<bool> g_enabled{LOG_ENABLED != 0};

using OutputFunc = void (*)(const char* msg, std::size_t len);
using FlushFunc = void (*)();

void defaultOutput(const char* msg, std::size_t len);
void defaultFlush();

void syncOutput(const char* msg, std::size_t len);
void syncFlush();

void asyncOutput(const char* msg, std::size_t len);
void asyncFlush();

std::atomic<OutputFunc> g_output(defaultOutput);
std::atomic<FlushFunc> g_flush(defaultFlush);

Logger::Config g_config;

std::unique_ptr<LogFile> g_syncLogFile;

std::unique_ptr<AsyncLogging> g_asyncLogging;
AsyncLogging* g_asyncLoggingRaw = nullptr;

bool g_syncToStdout = true;

const char* basename(const char* file)
{
    const char* slash = std::strrchr(file, '/');

    return slash ? slash + 1 : file;
}

Logger::Config sanitizeConfig(Logger::Config config)
{
    if (config.rollSize == 0) {
        config.rollSize = 64 * 1024 * 1024;
    }

    if (config.flushInterval <= 0) {
        config.flushInterval = 3;
    }

    if (config.maxBufferSize < 2) {
        config.maxBufferSize = 2;
    }

    return config;
}

// 默认输出
void defaultOutput(const char* msg, std::size_t len)
{
    if (msg && len > 0) {
        std::fwrite(msg, 1, static_cast<std::size_t>(len), stdout);
    }
}

void defaultFlush()
{
    std::fflush(stdout);
}

// 同步输出
void syncOutput(const char* msg, std::size_t len)
{
    if (!msg || len <= 0) {
        return;
    }

    if (g_syncToStdout) {
        std::fwrite(msg, 1, static_cast<std::size_t>(len), stdout);
    }

    if (g_syncLogFile) {
        g_syncLogFile->append(msg, len);
    }
}

void syncFlush()
{
    if (g_syncToStdout) {
        std::fflush(stdout);
    }

    if (g_syncLogFile) {
        g_syncLogFile->flush();
    }
}

// 异步输出
void asyncOutput(const char* msg, std::size_t len)
{
    AsyncLogging* logging = g_asyncLoggingRaw;

    if (logging) {
        logging->append(msg, len);
    }
}

void asyncFlush()
{
    AsyncLogging* logging = g_asyncLoggingRaw;

    if (logging) {
        logging->flush();
    }
}

}   // namespace

Logger::Logger(const char* file, int line, LogLevel level) : Logger(file, line, nullptr, level, 0) {}

Logger::Logger(const char* file, int line, const char* func, LogLevel level)
    : Logger(file, line, func, level, 0)
{}

Logger::Logger(const char* file, int line, const char* func, LogLevel level, int savedErrno)
    : level_(level), file_(basename(file)), func_(func ? func : "?"), line_(line), savedErrno_(savedErrno)
{
    /*
     * 日志前缀：
     *
     *   [INFO] 2026-06-01 19:00:00.123456 tid=1234
     */
    stream_ << '[' << levelName(level_) << "] ";

    formatTime();

    stream_ << "tid=" << CurrentThread::tid() << ' ';
}

Logger::~Logger()
{
    if (savedErrno_ != 0) {
        stream_ << "errno=" << savedErrno_ << " error=" << std::strerror(savedErrno_) << ' ';
    }

    finish();

    const LogStream::Buffer& buf = stream_.buffer();

    /*
     * 普通日志：
     *   再检查一次 isLevelEnabled。
     *
     * FATAL:
     *   必须输出。
     */
    if (level_ == FATAL || isLevelEnabled(level_)) {
        OutputFunc output = g_output.load(std::memory_order_acquire);
        output(buf.data(), buf.length());
    }

    /*
     * FATAL 输出后 flush 并终止。
     */
    if (level_ == FATAL) {
        FlushFunc flush = g_flush.load(std::memory_order_acquire);
        flush();
        std::abort();
    }
}

void Logger::formatTime()
{
    Timestamp now = Timestamp::now();

    // Timestamp::toFormattedString(true)
    // Example: 2026-05-16 09:08:58.313460
    stream_ << now.toFormattedString(true) << " ";
}

void Logger::finish()
{
    stream_ << " - " << file_ << "::" << func_ << ':' << line_ << '\n';
}

void Logger::init(const Config& rawConfig)
{
    Config config = sanitizeConfig(rawConfig);

    setLogLevel(config.level);
    enableLogging(config.enabled);

    if (g_asyncLogging) {
        g_asyncLogging->flush();
        g_asyncLoggingRaw = nullptr;
        g_asyncLogging.reset();
    }

    if (g_syncLogFile) {
        g_syncLogFile->flush();
        g_syncLogFile.reset();
    }

    g_config = config;

    /*
     * 异步模式。
     */
    if (config.async) {
        g_asyncLogging = std::make_unique<AsyncLogging>(config.logFile,
                                                        static_cast<off_t>(config.rollSize),
                                                        config.toStdout,
                                                        config.flushInterval,
                                                        config.maxBufferSize,
                                                        config.append);

        g_asyncLoggingRaw = g_asyncLogging.get();
        g_asyncLogging->start();

        g_output.store(asyncOutput, std::memory_order_release);
        g_flush.store(asyncFlush, std::memory_order_release);

        return;
    }

    /*
     * 同步模式。
     */
    g_syncToStdout = config.toStdout;

    if (!config.logFile.empty()) {
        g_syncLogFile = std::make_unique<LogFile>(config.logFile,
                                                  static_cast<off_t>(config.rollSize),
                                                  true,
                                                  config.flushInterval,
                                                  1024,
                                                  config.append);
    }

    g_output.store(syncOutput, std::memory_order_release);
    g_flush.store(syncFlush, std::memory_order_release);
}

void Logger::shutdown()
{
    // 先切回默认输出，避免 shutdown 后还有日志打到已经销毁的后端
    g_output.store(defaultOutput, std::memory_order_release);
    g_flush.store(defaultFlush, std::memory_order_release);

    if (g_asyncLogging) {
        g_asyncLogging->stop();
        g_asyncLoggingRaw = nullptr;
        g_asyncLogging.reset();
    }

    if (g_syncLogFile) {
        g_syncLogFile->flush();
        g_syncLogFile.reset();
    }

    std::fflush(stdout);
}

void Logger::flush()
{
    FlushFunc flush = g_flush.load(std::memory_order_acquire);
    flush();
}

Logger::LogLevel Logger::logLevel()
{
    return g_logLevel.load(std::memory_order_relaxed);
}

void Logger::setLogLevel(LogLevel level)
{
    if (level < TRACE) {
        level = TRACE;
    }

    if (level > OFF) {
        level = OFF;
    }

    g_logLevel.store(level, std::memory_order_relaxed);
}

bool Logger::enabled()
{
    return g_enabled.load(std::memory_order_relaxed);
}

void Logger::enableLogging(bool on)
{
    g_enabled.store(on, std::memory_order_relaxed);
}

bool Logger::isLevelEnabled(LogLevel level)
{
    return enabled() && level >= logLevel() && level < OFF;
}

std::uint64_t Logger::droppedLogs()
{
    AsyncLogging* logging = g_asyncLoggingRaw;

    if (logging) {
        return logging->droppedLogs();
    }

    return 0;
}

const char* Logger::levelName(LogLevel level)
{
    if (level < TRACE || level >= NUM_LOG_LEVELS) {
        return "UNKNOWN";
    }

    return kLogLevelName[level];
}



}   // namespace mymuduo