#include "Timestamp.h"

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <ctime>

using namespace mymuduo;

Timestamp::Timestamp() : microSecondsSinceEpoch_(0) {}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch) : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

Timestamp Timestamp::now()
{
    using namespace std::chrono;
    const auto now = system_clock::now().time_since_epoch();
    const auto us = duration_cast<microseconds>(now).count();
    return Timestamp(us);
}

Timestamp Timestamp::invalid()
{
    return Timestamp(0);
}

std::string Timestamp::toFormatedString(bool showMicroseconds) const
{
    const auto seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);

    tm tm_time{};
    localtime_r(&seconds, &tm_time);

    char buf[64]{};
    if (showMicroseconds) {
        const auto microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
        std::snprintf(buf,
                      sizeof(buf),
                      "%04d-%02d-%02d %02d:%02d:%02d.%06d",
                      tm_time.tm_year + 1900,
                      tm_time.tm_mon + 1,
                      tm_time.tm_mday,
                      tm_time.tm_hour,
                      tm_time.tm_min,
                      tm_time.tm_sec,
                      microseconds);
    }
    else {
        std::snprintf(buf,
                      sizeof(buf),
                      "%04d-%02d-%02d %02d:%02d:%02d",
                      tm_time.tm_year + 1900,
                      tm_time.tm_mon + 1,
                      tm_time.tm_mday,
                      tm_time.tm_hour,
                      tm_time.tm_min,
                      tm_time.tm_sec);
    }

    return buf;
}

std::string Timestamp::toString() const
{
    char buf[32]{};
    int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
    int64_t microSeconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;

    std::snprintf(buf, sizeof(buf), "%" PRId64 ".%06" PRId64 " ", seconds, microSeconds);
    return buf;
}