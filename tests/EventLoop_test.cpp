#include "EventLoop.h"
#include "test_util.h"

#include <atomic>
#include <thread>

using namespace mymuduo;

int main()
{
    quietLogger();

    EventLoop loop;
    int immediate = 0;
    int queued = 0;
    int nested = 0;
    std::atomic<int> crossThread{0};

    // 同线程 runInLoop 应立即执行。
    loop.runInLoop([&] { immediate = 1; });
    CHECK_EQ(immediate, 1);

    loop.queueInLoop([&] { queued = 1; });

    loop.queueInLoop([&] {
        loop.queueInLoop([&] { nested = 1; });
    });

    std::thread t([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        loop.queueInLoop([&] { crossThread = 1; });
    });

    loop.runAfter(0.100, [&] { loop.quit(); });
    loop.loop();
    t.join();

    CHECK_EQ(queued, 1);
    CHECK_EQ(nested, 1);
    CHECK_EQ(crossThread.load(), 1);

    std::cout << "EventLoop_test passed\n";
    return 0;
}
