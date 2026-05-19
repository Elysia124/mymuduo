#pragma once

#include <cstdint>
namespace mymuduo {
class Timer;

// 用于取消定时器
class TimerId
{
public:
    friend class TimerQueue;

    TimerId() = default;

    bool valid() const { return sequence_ > 0; }

private:
    TimerId(int64_t sequence) : sequence_(sequence) {}
    int64_t sequence_{0};
};
}   // namespace mymuduo