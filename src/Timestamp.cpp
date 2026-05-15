#include "Timestamp.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>

using namespace mymuduo;
Timestamp::Timestamp() : microSecondsSinceEpoch_(0) {}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch) : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

Timestamp Timestamp::now()
{
    using namespace std::chrono;
    auto now = system_clock::now().time_since_epoch();
    auto us = duration_cast<microseconds>(now).count();
    return Timestamp(us);
}

std::string Timestamp::toString() const
{
    auto seconds = static_cast<time_t>(microSecondsSinceEpoch_ / 1000000);

    tm tm_time{};
    localtime_r(&seconds, &tm_time);

    char buf[128]{};
    snprintf(buf,
             sizeof(buf),
             "%04d%02d%02d %02d:%02d:%02d",
             tm_time.tm_year + 1900,
             tm_time.tm_mon + 1,
             tm_time.tm_mday,
             tm_time.tm_hour,
             tm_time.tm_min,
             tm_time.tm_sec);

    return buf;
}