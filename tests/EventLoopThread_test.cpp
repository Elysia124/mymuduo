#include "EventLoop.h"
#include "EventLoopThread.h"
#include "CurrentThread.h"
#include "test_util.h"

#include <atomic>
#include <condition_variable>
#include <mutex>

using namespace mymuduo;

int main()
{
    quietLogger();

    std::atomic<bool> initCalled{false};
    pid_t loopTid = 0;

    std::mutex m;
    std::condition_variable cv;
    bool callbackRan = false;
    pid_t callbackTid = 0;

    {
        EventLoopThread loopThread([&](EventLoop* loop) {
            initCalled = true;
            loopTid = CurrentThread::tid();
            CHECK_TRUE(loop->isInLoopThread());
        }, "EventLoopThread_test");

        EventLoop* loop = loopThread.startLoop();
        CHECK_TRUE(loop != nullptr);
        CHECK_TRUE(initCalled.load());

        loop->queueInLoop([&] {
            callbackTid = CurrentThread::tid();

            {
                std::lock_guard<std::mutex> lock(m);
                callbackRan = true;
            }

            cv.notify_one();
            loop->quit();
        });

        {
            std::unique_lock<std::mutex> lock(m);
            CHECK_TRUE(cv.wait_for(lock, std::chrono::seconds(1), [&] {
                return callbackRan;
            }));
        }

        CHECK_TRUE(callbackTid != 0);
        CHECK_EQ(callbackTid, loopTid);

        // 离开这个作用域时，loopThread 先析构并 join。
        // m / cv / callbackRan / callbackTid 在外层，仍然活着。
    }

    std::cout << "EventLoopThread_test passed\n";
    return 0;
}