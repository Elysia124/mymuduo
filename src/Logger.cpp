#include "Logger.h"
#include "Timestamp.h"

#include "CurrentThread.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace mymuduo {

namespace {

const char* LogLevelName[Logger::NUM_LOG_LEVELS] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

std::atomic<Logger::LogLevel> g_logLevel{Logger::INFO};

std::mutex g_mutex_;

void defaultOutput(const char* msg, size_t len)
{
    std::fwrite(msg, 1, len, stdout);
}

void defaultFlush()
{
    std::fflush(stdout);
}

Logger::OutputFunc g_output = defaultOutput;
Logger::FlushFunc g_flush = defaultFlush;

const char* basename(const char* file)
{
    const char* slash = std::strrchr(file, '/');

#ifdef _WIN32
    const char* backslash = std::strrchr(file, '\\');
    if (!slash || backslash > slash) {
        slash = backslash;
    }
#endif

    return slash ? slash + 1 : file;
}

}   // namespace

Logger::Logger(const char* file, int line, LogLevel level) : level_(level), file_(basename(file)), line_(line)
{
    stream_ << '[' << LogLevelName[level_] << "] ";

    formatTime();

    auto tid = CurrentThread::tid();

    stream_ << "tid=" << tid << " ";
}

Logger::~Logger()
{
    finish();

    const LogStream::Buffer& buf = stream_.buffer();

    {
        std::lock_guard<std::mutex> lock(g_mutex_);
        g_output(buf.data(), static_cast<size_t>(buf.length()));
        if (level_ == FATAL) {
            g_flush();
        }
    }

    if (level_ == FATAL) {
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
    stream_ << " - " << file_ << ':' << line_ << '\n';
}

Logger::LogLevel Logger::logLevel()
{
    return g_logLevel.load(std::memory_order_relaxed);
}

void Logger::setLogLevel(LogLevel level)
{
    g_logLevel.store(level, std::memory_order_relaxed);
}

void Logger::setOutput(OutputFunc output)
{
    g_output = output;
}

void Logger::setFlush(FlushFunc flush)
{
    g_flush = flush;
}

}   // namespace mymuduo