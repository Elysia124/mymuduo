#pragma once

#include "CurrentThread.h"
#include "Timestamp.h"
#include "Logger.h"
#include "noncopyable.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <sys/types.h>
#include <vector>
namespace mymuduo {

class Channel;
class Poller;

// 事件循环类 主要包含两个大模块 Channel Poller(Epoll的抽象)
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();

    // 退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    // 在当前loop中执行
    void runInLoop(Functor cb);

    // 把 cb 放入队列中，唤醒loop所在的线程，执行cb
    void queueInLoop(Functor cb);

    // 唤醒 loop所在的线程
    void wakeup() const;


    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // 判断 eventloop 对象是否在自己的线程中
    /*
        客户端连接时，由 MainReactor 选择一个 SubReactor 中 EventLoopThreadPool 中一个 EventLoop 执行 loop->xxx
        此时，是在 MainReactor 中执行的 loop->xxx，而不是在 EventLoop 所属的 thread 中执行

        EventLoop内部可能将耗时的任务通过用户自定义的线程池中选一个线程去执行，执行完毕后可能需要回复操作等
        这些操作不能由执行耗时任务的线程去完成，必须由 EventLoop 自己所在的线程去执行
    */
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead() const;    // wake up
    void doPendingFunctors();   // 执行回调

    void assertInLoopThread() const
{
    if (!isInLoopThread()) {
        LOG_FATAL("EventLoop used from wrong thread: owner_tid=%d current_tid=%d", threadId_, CurrentThread::tid());
    }
}

    using ChannelList = std::vector<Channel*>;

    std::atomic<bool> looping_;
    std::atomic<bool> quit_;
    // std::atomic<bool> eventHandling_;
    std::atomic<bool> callingPendingFunctors_;   // 标识当前loop是否有需要执行的回调操作
    const pid_t threadId_;                       // 记录 eventloop 所在线程的id
    Timestamp pollReturnTime_;                   // poller 返回发生事件的channels的时间
    std::unique_ptr<Poller> poller_;
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;
    Channel* currentActiveChannel_;

    std::vector<Functor> pendingFunctors_;   // 存储loop需要执行的所有回调操作
    std::mutex mutex_;                       // 互斥锁，保护 pendingFunctors_ 的线程安全
};
}   // namespace mymuduo