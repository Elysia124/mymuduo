#include "Logger.h"
#include "CurrentThread.h"
#include "Timestamp.h"

#include <cstdio>

using namespace mymuduo;

Logger& Logger::getInstance()
{
    static Logger logger;
    return logger;
}

const char* Logger::levelName(LogLevel level)
{
    switch (level) {
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO: return "INFO ";
    case LogLevel::ERROR: return "ERROR";
    case LogLevel::FATAL: return "FATAL";
    }
    return "UNKNOWN";
}

const char* Logger::basename(const char* path)
{
    if (path == nullptr) {
        return "unknown";
    }

    const char* base = path;
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

std::string_view Logger::trimTrailingNewline(std::string_view msg)
{
    while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) { msg.remove_suffix(1); }
    return msg;
}

void Logger::log(LogLevel level, const char* file, const char* func, std::string_view msg) const
{
    const std::string time = Timestamp::now().toString(true);

    msg = trimTrailingNewline(msg);
    const char* safeFunc = func == nullptr ? "unknown" : func;
    std::FILE* out = (level == LogLevel::ERROR || level == LogLevel::FATAL) ? stderr : stdout;

    std::lock_guard<std::mutex> lock(mutex_);
    std::fprintf(out,
                 "[%s] %s [tid=%d] [%-s::%-s] - [%.*s]\n",
                 levelName(level),
                 time.c_str(),
                 CurrentThread::tid(),
                 basename(file),
                 safeFunc,
                 static_cast<int>(msg.size()),
                 msg.data());
    std::fflush(out);
}
