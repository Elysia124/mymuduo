#pragma once

#include "net/Callbacks.h"
#include "net/Channel.h"
#include "net/Timestamp.h"
#include "net/noncopyable.h"
#include <cstdint>
#include <memory>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mymuduo {
class EventLoop;
class Timer;
class TimerId;

class TimerQueue : noncopyable
{
public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    TimerId addTimer(TimerCallback cb, Timestamp when, double interval);

    void cancel(TimerId timer);

private:
    // 用于按照过期时间排序定时器
    using TimerPtr = std::unique_ptr<Timer>;

    struct SearchKey
    {
        Timestamp expiration;
        int64_t sequence;
    };

    /*
     *TimerList 的比较器
     *timer的排序规则
     *按 expiration 升序，expiration 相同则按 sequence 升序排列
     *支持异构查找(set.find()支持传入裸指针)
     *set 判断 key 是否相等用的不是 == 而是比较器：!comp(a,b) && !comp(b,a)
     */
    struct TimerPtrLess
    {
        using is_transparent = void;   // 声明为透明比较器

    private:
        template<typename T>
        static auto getExpiration(const T& item)
        {
            if constexpr (std::is_same_v<std::decay_t<T>, SearchKey>) {
                return item.expiration;
            }
            else {
                return item->expiration();
            }
        }

        template<typename T>
        static auto getSequence(const T& item)
        {
            if constexpr (std::is_same_v<std::decay_t<T>, SearchKey>) {
                return item.sequence;
            }
            else {
                return item->sequence();
            }
        }

    public:
        template<typename T1, typename T2>
        static bool lessTimer(const T1& lhs, const T2& rhs)
        {
            if (getExpiration(lhs) < getExpiration(rhs)) {
                return true;
            }
            if (getExpiration(rhs) < getExpiration(lhs)) {
                return false;
            }
            return getSequence(lhs) < getSequence(rhs);
        }

        template<typename T1, typename T2>
        bool operator()(const T1& lhs, const T2& rhs) const
        {
            return lessTimer(lhs, rhs);
        }
    };

    using TimerList = std::set<TimerPtr, TimerPtrLess>;

    /*
     *判断 TimerId 是否有效
     *根据 sequence 找到对应的 Timer*
     *不拥有 Timer，Timer 生命周期由 TimerList 中的 TimerPtr 管理
     */
    using ActiveTimerMap = std::unordered_map<int64_t, Timer*>;

    /*
     *记录正在执行 expired callbacks 中被取消的 repeat timer
     *key 为 sequence
     */
    using CancelingTimerSet = std::unordered_set<int64_t>;

    void addTimerInLoop(TimerPtr timer);
    void cancelInLoop(TimerId timerId);

    // timerfd 可读时调用
    void handleRead();

    // 得到所有已过期的定时器
    std::vector<TimerPtr> getExpired(Timestamp now);

    void reset(std::vector<TimerPtr>&& expired, Timestamp now);

    bool insert(TimerPtr timer);

    EventLoop* loop_;
    const int timerfd_;
    Channel timerfdChannel_;

    /*
     *真正拥有 timer
     *按 expiration + sequence 排序
     */
    TimerList timers_;

    // 用于取消定时器
    ActiveTimerMap activeTimersMap_;
    bool callingExpiredTimers_;

    /*
     * 回调执行期间被 cancel 的 Timer 的 sequence。
     */
    CancelingTimerSet cancelingTimers_;
};
}   // namespace mymuduo