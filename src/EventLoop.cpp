#include "EventLoop.h"
#include "Channel.h"
#include "CurrentThread.h"
#include "Logger.h"
#include "Poller.h"
#include "TimerQueue.h"
#include "Timestamp.h"
#include <climits>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace mymuduo;

namespace {

// 防止一个线程创建多个 eventloop 对象
thread_local EventLoop* t_loopInThisThread = nullptr;

// 默认超时时间
const int kPollTimeMs = 10000;

// 创建一个 wakeupfd, 用于唤醒 subReactor 处理新来的 channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG_FATAL("eventfd create failed: errno=%d error=%s", errno, strerror(errno));
    }
    return evtfd;
}
}   // namespace

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(std::make_unique<Channel>(this, wakeupFd_))
    , currentActiveChannel_(nullptr)
{
    LOG_DEBUG("EventLoop created loop=%p owner_tid=%d", static_cast<void*>(this), threadId_);

    if (t_loopInThisThread != nullptr) {
        LOG_FATAL("another EventLoop already exists in this thread: existing_loop=%p tid=%d",
                  static_cast<void*>(t_loopInThisThread),
                  threadId_);
    }
    else {
        t_loopInThisThread = this;
    }

    // 设置wakeupfd的时间类型及发生事件后的回调操作
    wakeupChannel_->setReadCallback([this](Timestamp) { handleRead(); });

    // 每个 eventloop 监听 wakeupfd 的读事件
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 唤醒 eventloop
void EventLoop::handleRead() const
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));

    if (n != sizeof(one)) {
        LOG_ERROR("eventfd read unexpected bytes=%zd expected=%zu", n, sizeof(one));
    }
}


void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop start loop=%p owner_tid=%d", static_cast<void*>(this), threadId_);

    while (!quit_) {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);

        // 对有事件的 channel 调用 handleEvent 处理相应的事件
        for (auto* channel : activeChannels_) { channel->handleEvent(pollReturnTime_); }

        // 执行当前 eventloop 事件循环需要处理的回调操作
        doPendingFunctors();
    }

    LOG_INFO("EventLoop stop loop=%p owner_tid=%d", static_cast<void*>(this), threadId_);
    looping_ = false;
}

void EventLoop::quit()
{
    quit_ = true;

    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) {
        cb();
    }
    else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

void EventLoop::wakeup() const
{
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof(one));

    if (n != sizeof(one)) {
        LOG_ERROR("eventfd write unexpected bytes=%zd expected=%zu", n, sizeof(one));
    }
}


void EventLoop::updateChannel(Channel* channel)
{
    assertInLoopThread();
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
    assertInLoopThread();
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (auto& functor : functors) { functor(); }

    callingPendingFunctors_ = false;
}

void EventLoop::assertInLoopThread() const
{
    if (!isInLoopThread()) {
        LOG_FATAL("EventLoop used from wrong thread: owner_tid=%d current_tid=%d", threadId_, CurrentThread::tid());
    }
}

TimerId EventLoop::runAt(Timestamp time, TimerCallback cb)
{
    return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

TimerId EventLoop::runAfter(double delay, TimerCallback cb)
{
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time, std::move(cb));
}

TimerId EventLoop::runEvery(double interval, TimerCallback cb)
{
    Timestamp time(addTime(Timestamp::now(), interval));
    return timerQueue_->addTimer(std::move(cb), time, interval);
}

void EventLoop::cancle(TimerId timerId)
{
    return timerQueue_->cancel(timerId);
}