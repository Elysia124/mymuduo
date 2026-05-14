#include "Timestamp.h"
#include <cstdint>
#include <cstdio>
#include <ctime>

using namespace mymuduo;
Timestamp::Timestamp() : microSecondsSinceEpoch_(0) {}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch) : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

Timestamp now()
{
    return Timestamp(time(nullptr));
}

std::string Timestamp::toString() const
{
    char buf[128]{};
    tm* time = localtime(&microSecondsSinceEpoch_);
    snprintf(buf,
             128,
             "%4d%02d%02d %02d:%02d:%02d",
             time->tm_year + 1900,
             time->tm_mon + 1,
             time->tm_mday,
             time->tm_hour,
             time->tm_min,
             time->tm_sec);
    return buf;
}