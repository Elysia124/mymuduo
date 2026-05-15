#include "EventLoopThread.h"
#include "EventLoop.h"
#include "Thread.h"
#include <mutex>
#include <utility>

using namespace mymuduo;

EventLoopThread::EventLoopThread(ThreadInitCallback cb, std::string name)
    : loop_(nullptr), exiting_(false), thread_([this]() { threadFunc(); }, std::move(name)), callback_(std::move(cb))
{}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;

    EventLoop* loop = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop = loop_;
    }

    if (loop != nullptr) {
        loop->quit();
    }

    if (thread_.joinable()) {
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop()
{
    thread_.start();

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr) { cond_.wait(lock); }
        loop = loop_;
    }

    return loop;
}

// 负责开一个 eventloop 线程
void EventLoopThread::threadFunc()
{
    EventLoop loop;

    if (callback_) {
        callback_(&loop);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();   // loop 会一直阻塞在事件循环，所以局部变量 loop 还不会被销毁

    std::lock_guard<std::mutex> lock(mutex_);
    loop_ = nullptr;
}