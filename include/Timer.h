#pragma once

#include "Callbacks.h"
#include "Timestamp.h"
#include "noncopyable.h"
#include <atomic>
#include <cstdint>
#include <utility>

namespace mymuduo {

// 封装一个具体的定时任务
class Timer : noncopyable
{
public:
    Timer(TimerCallback cb, Timestamp when, double interval)
        : timerCallback_(std::move(cb))
        , expiration_(when)
        , interval_(interval)
        , repeat_(interval > 0.0)
        , sequence_(s_numCreated_.fetch_add(1, std::memory_order_relaxed))
    {}

    // 执行定时器任务
    void run() const { timerCallback_(); }

    Timestamp expiration() const { return expiration_; }

    bool repeat() const { return repeat_; }

    int64_t sequence() const { return sequence_; }

    // reset new_expiration = now + old_expiration
    void restart(Timestamp now);

    static int64_t numCreated() { return s_numCreated_.load(std::memory_order_relaxed); }

private:
    TimerCallback timerCallback_;                          // 定时器执行的任务回调
    Timestamp expiration_;                                 // 到期时间
    const double interval_;                                // 重复间隔
    const bool repeat_;                                    // 是否重复
    const int64_t sequence_;                               // 定时器序号
    inline static std::atomic<int64_t> s_numCreated_{1};   // 自 Timer 类创建依赖产生了多少个 Timer，用于生成定时器序号
};
}   // namespace mymuduo