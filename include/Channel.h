#pragma once

#include "Timestamp.h"
#include "noncopyable.h"
#include <cstdint>
#include <functional>
#include <memory>
namespace mymuduo {
class EventLoop;
;

/*
Channel 封装了 sockfd 和其感兴趣事件 event，如 EPOLLIN, EPOLLOUT 事件
*/
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    // fd 得到 Poller 通知以后，处理事件
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void seterrorallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 防止当 channel 被手动 remove 后，channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_; }
    uint32_t events() const { return events_; }
    void setRevents(uint32_t revt) { revents_ = revt; }

    // 设置 fd 相应的事件状态
    void enabelReading()
    {
        events_ |= kReadEvent;
        update();
    }
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    // 返回 fd 当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isReaing() const { return static_cast<bool>(events_ & kReadEvent); }
    bool isWriting() const { return static_cast<bool>(events_ & kWriteEvent); }

    int index() const { return index_; }
    void setIndex(int idx) { index_ = idx; }

    // 返回当前 channel 属于哪个 EventLoop
    EventLoop* ownerLoop() { return loop_; }

    void remove();


private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const uint32_t kNoneEvent;
    static const uint32_t kReadEvent;
    static const uint32_t kWriteEvent;

    EventLoop* loop_;   // 事件循环
    const int fd_;      // 文件描述符，Poller的监听对象
    uint32_t events_;        // 向Poller注册的感兴趣的事件
    uint32_t revents_;       // Poller返回的实际发生的事件
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;

    // 系列回调函数
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
}   // namespace mymuduo