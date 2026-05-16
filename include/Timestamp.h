#pragma once

#include <cstdint>
#include <string>
namespace mymuduo {
class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);

    static Timestamp now();

    // Example: 2026-05-16 09:08:58.313460
    std::string toString(bool showMicroseconds = true) const;

private:
    static constexpr int kMicroSecondsPerSecond = 1000 * 1000;

    int64_t microSecondsSinceEpoch_;
};
}   // namespace mymuduo