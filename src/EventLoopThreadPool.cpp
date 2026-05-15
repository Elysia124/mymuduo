#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "EventLoopThread.h"
#include <algorithm>
#include <cstdio>
#include <memory>

using namespace mymuduo;

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg)
    : baseLoop_(baseLoop), name_(nameArg), started_(false), numThreads_(0), next_(0)
{}

EventLoopThreadPool::~EventLoopThreadPool() {}


void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
    started_ = true;

    for (int i = 0; i < numThreads_; ++i) {
        std::string threadName = name_ + std::to_string(i);
        auto t = std::make_unique<EventLoopThread>(cb, threadName);
        EventLoop* loop = t->startLoop();   // 创建一个新线程执行 eventloop
        threads_.push_back(std::move(t));
        loops_.push_back(loop);   // 将该 loop 加入到 loops 中
    }

    if (numThreads_ == 0 && cb) {
        cb(baseLoop_);
    }
}

EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop* loop = baseLoop_;

    if (!loops_.empty()) {
        // 轮询获取下一个 loop
        loop = loops_[next_];
        ++next_;
        if (static_cast<size_t>(next_) >= loops_.size()) {
            next_ = 0;
        }
    }

    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    if (loops_.empty()) {
        return {baseLoop_};
    }
    else {
        return loops_;
    }
}
