#include "EventLoop.h"
#include "TimerId.h"
#include "test_util.h"

#include <atomic>

using namespace mymuduo;

int main()
{
    quietLogger();

    EventLoop loop;

    bool earliestFired = false;
    bool nextFired = false;
    bool lateFired = false;
    bool canceledByOtherFired = false;
    bool onceFired = false;
    std::atomic<int> repeatCount{0};

    // 取消无效 TimerId：不应该崩溃。
    loop.cancel(TimerId{});

    // 取消当前最早 timer。TimerQueue 即使不立即重设 timerfd，也不应该执行它。
    TimerId earliest = loop.runAfter(0.010, [&] {
        earliestFired = true;
    });
    loop.cancel(earliest);

    loop.runAfter(0.020, [&] {
        nextFired = true;
    });

    // 已经执行过的一次性 timer，再 cancel 不应该崩溃。
    TimerId once = loop.runAfter(0.030, [&] {
        onceFired = true;
    });
    loop.runAfter(0.050, [&] {
        loop.cancel(once);
    });

    // 在一个 timer 回调里取消另一个还没到期的 timer。
    TimerId canceledByOther = loop.runAfter(0.090, [&] {
        canceledByOtherFired = true;
    });
    loop.runAfter(0.040, [&] {
        loop.cancel(canceledByOther);
    });

    // 重复 timer 在自己的回调里取消自己。
    TimerId repeat;
    repeat = loop.runEvery(0.015, [&] {
        int n = repeatCount.fetch_add(1, std::memory_order_relaxed) + 1;
        if (n == 3) {
            loop.cancel(repeat);
        }
    });

    loop.runAfter(0.080, [&] {
        lateFired = true;
    });

    loop.runAfter(0.160, [&] {
        loop.quit();
    });

    loop.loop();

    CHECK_TRUE(!earliestFired);
    CHECK_TRUE(nextFired);
    CHECK_TRUE(onceFired);
    CHECK_TRUE(!canceledByOtherFired);
    CHECK_EQ(repeatCount.load(std::memory_order_relaxed), 3);
    CHECK_TRUE(lateFired);

    std::cout << "TimerQueue_cancel_edge_test passed\n";
    return 0;
}
