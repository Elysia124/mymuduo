#include "net/EventLoop.h"
#include "net/EventLoopThreadPool.h"
#include "test_util.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <set>

using namespace mymuduo;

int main()
{
    quietLogger();

    EventLoop baseLoop;

    std::mutex m;
    std::condition_variable cv;
    int ran = 0;

    std::atomic<int> initCount{0};

    {
        EventLoopThreadPool pool(&baseLoop, "pool_test");
        pool.setThreadNum(3);

        pool.start([&](EventLoop*) {
            initCount.fetch_add(1, std::memory_order_relaxed);
        });

        CHECK_TRUE(pool.started());
        CHECK_EQ(initCount.load(std::memory_order_relaxed), 3);

        EventLoop* l1 = pool.getNextLoop();
        EventLoop* l2 = pool.getNextLoop();
        EventLoop* l3 = pool.getNextLoop();
        EventLoop* l4 = pool.getNextLoop();

        CHECK_TRUE(l1 != nullptr);
        CHECK_TRUE(l2 != nullptr);
        CHECK_TRUE(l3 != nullptr);
        CHECK_TRUE(l1 != &baseLoop);
        CHECK_TRUE(l1 != l2);
        CHECK_TRUE(l2 != l3);
        CHECK_EQ(l4, l1);

        auto loops = pool.getAllLoops();
        CHECK_EQ(loops.size(), 3u);

        for (EventLoop* loop : loops) {
            loop->queueInLoop([&] {
                {
                    std::lock_guard<std::mutex> lock(m);
                    ++ran;
                }

                cv.notify_one();
            });
        }

        {
            std::unique_lock<std::mutex> lock(m);
            CHECK_TRUE(cv.wait_for(lock, std::chrono::seconds(1), [&] {
                return ran == 3;
            }));
        }

        for (EventLoop* loop : loops) {
            loop->quit();
        }

        // 离开这个作用域时，pool 先析构。
        // pool 析构会析构内部 EventLoopThread，并等待线程退出。
        // m / cv / ran 仍然活着。
    }

    std::cout << "EventLoopThreadPool_test passed\n";
    return 0;
}