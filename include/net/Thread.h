#pragma once

#include "net/noncopyable.h"
#include <atomic>
#include <functional>
#include <sched.h>
#include <string>
#include <thread>
namespace mymuduo {
class Thread : noncopyable
{
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc, std::string name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }

    pid_t tid() const { return tid_; }

    const std::string& name() const { return name_; }

    static int numCreated() { return numCreated_.load(std::memory_order_relaxed); }

    bool joinable() const noexcept { return thread_.joinable(); };

private:
    void setDefaultName();
    bool started_;
    bool joined_;
    std::thread thread_;
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;

    inline static std::atomic<int> numCreated_ = 0;
};
}   // namespace mymuduo