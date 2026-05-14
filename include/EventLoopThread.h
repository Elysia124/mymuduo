#pragma once

#include "Thread.h"
#include "noncopyable.h"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
namespace mymuduo {

class EventLoop;

class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(), const std::string& name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();

private:
    void threadFunc();

    EventLoop* loop_;
    bool exiting_;
    Thread thread_;      // 执行 eventloop 的线程
    std::mutex mutex_;   // 保护 thread_ 和 threadFunc 对 loop_ 的并发访问
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};
}   // namespace mymuduo