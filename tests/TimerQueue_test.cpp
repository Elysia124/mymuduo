#include "net/EventLoop.h"
#include "test_util.h"

#include <vector>

using namespace mymuduo;

int main()
{
    quietLogger();

    EventLoop loop;
    std::vector<int> order;
    int everyCount = 0;
    bool cancelledFired = false;
    bool nestedFired = false;

    loop.runAfter(0.030, [&] { order.push_back(3); });
    loop.runAfter(0.010, [&] { order.push_back(1); });
    loop.runAfter(0.020, [&] { order.push_back(2); });

    TimerId cancelled = loop.runAfter(0.040, [&] { cancelledFired = true; });
    loop.cancel(cancelled);

    TimerId every;
    every = loop.runEvery(0.010, [&] {
        ++everyCount;
        if (everyCount == 3) {
            loop.cancel(every);
        }
    });

    loop.runAfter(0.050, [&] {
        loop.runAfter(0.010, [&] { nestedFired = true; });
    });

    loop.runAfter(0.150, [&] { loop.quit(); });
    loop.loop();

    CHECK_EQ(order.size(), 3u);
    CHECK_EQ(order[0], 1);
    CHECK_EQ(order[1], 2);
    CHECK_EQ(order[2], 3);
    CHECK_EQ(everyCount, 3);
    CHECK_TRUE(!cancelledFired);
    CHECK_TRUE(nestedFired);

    std::cout << "TimerQueue_test passed\n";
    return 0;
}
