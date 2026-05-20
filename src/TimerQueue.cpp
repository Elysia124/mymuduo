#include "TimerQueue.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Timer.h"
#include "TimerId.h"
#include "Timestamp.h"
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <memory>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace mymuduo::detail {
int createTimerfd()
{
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        LOG_FATAL << "create timerfd failed errno=" << errno << ", error=" << strerror(errno);
    }
    return timerfd;
}

timespec howMuchTimeFromNow(Timestamp when)
{
    auto microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
    if (microseconds < 100) {
        // 保证最少100微秒
        microseconds = 100;
    }

    timespec ts;
    ts.tv_sec = static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);   // 秒
    ts.tv_nsec =
        static_cast<long>(microseconds % Timestamp::kMicroSecondsPerSecond * 1000);   // 纳秒(表示不足一秒的部分)
    return ts;
}

void readTimerfd(int timerfd)
{
    uint64_t howmany = 0;
    ssize_t n = ::read(timerfd, &howmany, sizeof(howmany));

    LOG_TRACE << "Timer=" << timerfd << " has timed out " << howmany << " times";

    if (n != sizeof(howmany)) {
        LOG_ERROR << "timer read() reads " << n << " bytes instead of 8 errno=" << errno << ", error=" << strerror(errno);
    }
}

// 对 timerfd 设置一个新的 expiration
void resetTimerfd(int timerfd, Timestamp expiration)
{
    itimerspec newValue{};
    itimerspec oldValue{};
    newValue.it_value = howMuchTimeFromNow(expiration);
    if (::timerfd_settime(timerfd, 0, &newValue, &oldValue) < 0) {
        LOG_FATAL << "timerfd=" << timerfd << " set time failed errno=" << errno << ", error=" << strerror(errno);
    }
}
}   // namespace mymuduo::detail

using namespace mymuduo;
using namespace mymuduo::detail;

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop), timerfd_(createTimerfd()), timerfdChannel_(loop, timerfd_), callingExpiredTimers_(false)
{
    timerfdChannel_.setReadCallback([this](Timestamp) { handleRead(); });
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);
}

TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval)
{
    // 外部 new，进入线程后交给 unique_ptr 接管
    // 因为 std::function 要求内部包裹的对象必须是可拷贝的，而 unique_ptr 是 move-only
    auto* timer = new Timer(std::move(cb), when, interval);
    int64_t sequence = timer->sequence();
    loop_->runInLoop([this, timer] { addTimerInLoop(TimerPtr(timer)); });
    return {sequence};
}

void TimerQueue::cancel(TimerId timer)
{
    if (!timer.valid()) {
        return;
    }

    loop_->runInLoop([this, timer] { cancelInLoop(timer); });
}

void TimerQueue::addTimerInLoop(TimerPtr timer)
{
    loop_->assertInLoopThread();

    Timestamp expiration = timer->expiration();
    bool earliestChanged = insert(std::move(timer));
    if (earliestChanged) {
        resetTimerfd(timerfd_, expiration);
    }
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimersMap_.size());

    auto activeIt = activeTimersMap_.find(timerId.sequence_);
    if (activeIt != activeTimersMap_.end()) {   // 该 timer 有效
        Timer* timer = activeIt->second;

        auto timerIt = timers_.find(timer);   // 异构查找

        assert(timerIt != timers_.end());

        activeTimersMap_.erase(activeIt);
        timers_.erase(timerIt);
    }
    else if (callingExpiredTimers_) {
        // 如果当前正在执行定时器任务回调
        // 就不会直接在 TimerList 中删除定时器
        // 而是加入到 CancelingTimerSet 中等待后续处理
        cancelingTimers_.insert(timerId.sequence_);
    }
}

void TimerQueue::handleRead()
{
    loop_->assertInLoopThread();
    Timestamp now(Timestamp::now());
    readTimerfd(timerfd_);

    auto expired = getExpired(now);   // 拿到所有到期的定时器

    callingExpiredTimers_ = true;   // 标记当前正在执行定时器任务回调
    cancelingTimers_.clear();

    for (const auto& timer : expired) {
        // 依次执行定时器任务回调
        timer->run();
    }

    callingExpiredTimers_ = false;
    reset(std::move(expired), now);
    cancelingTimers_.clear();
}

std::vector<TimerQueue::TimerPtr> TimerQueue::getExpired(Timestamp now)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimersMap_.size());

    std::vector<TimerQueue::TimerPtr> expired;

    // 构造查找键，expiration=now, sequence无穷大
    SearchKey searchKey{now, std::numeric_limits<int64_t>::max()};

    auto end = timers_.lower_bound(searchKey);

    for (auto it = timers_.begin(); it != end;) {
        activeTimersMap_.erase(it->get()->sequence());   // 从 activeTimersMap 中删除

        auto node = timers_.extract(it++);   // 会同时从timers中删除
        expired.push_back(std::move(node.value()));
    }

    assert(timers_.size() == activeTimersMap_.size());
    return expired;
}

void TimerQueue::reset(std::vector<TimerPtr>&& expired, Timestamp now)
{
    loop_->assertInLoopThread();
    Timestamp nextExpire;

    for (auto& timer : expired) {
        int64_t sequence = timer->sequence();

        // 如果是重复定时器且未被取消
        if (timer->repeat() && cancelingTimers_.find(sequence) == cancelingTimers_.end()) {
            timer->restart(now);        // 设置新的过期时间
            insert(std::move(timer));   // 重新插入回 timers
        }
    }

    // 重新将 expiration 最小的定时器的超时时间设置给 timerfd
    if (!timers_.empty()) {
        nextExpire = timers_.begin()->get()->expiration();
        if (nextExpire.valid()) {
            resetTimerfd(timerfd_, nextExpire);
        }
    }
}

bool TimerQueue::insert(TimerPtr timer)
{
    loop_->assertInLoopThread();
    assert(timer);
    assert(timers_.size() == activeTimersMap_.size());

    bool earliestChanged = false;
    Timer* rawTimer = timer.get();
    auto it = timers_.begin();

    if (it == timers_.end() || TimerPtrLess::lessTimer(rawTimer, it->get())) {
        // 如果 timers 为空或新加入的 timer(expiration和sequence) 小于 timers 里最小的
        earliestChanged = true;
    }

    auto activeResult = activeTimersMap_.emplace(rawTimer->sequence(), rawTimer);
    assert(activeResult.second);
    (void)activeResult;


    auto timerResult = timers_.insert(std::move(timer));
    assert(timerResult.second);
    (void)timerResult;

    assert(timers_.size() == activeTimersMap_.size());
    return earliestChanged;
}