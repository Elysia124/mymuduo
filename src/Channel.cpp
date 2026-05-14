#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include <cstdint>
#include <memory>
#include <sys/epoll.h>

using namespace mymuduo;

const uint32_t kNoneEvent = 0;
const uint32_t kReadEvent = EPOLLIN | EPOLLPRI;
const uint32_t kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop* loop, int fd) : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false) {}

Channel::~Channel() {}

void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_ = obj;
    tied_ = true;
}

// 在 channel 所属的 eventLoop 中删除当前的 channel
void Channel::remove()
{
    loop_->removeChannel(this);
}

/*
当改变 channel 里的 fd 的感兴趣事件后，update负责在 poller 里更改 fd 相应的事件(epoll_ctl)
*/
void Channel::update() {
    // 通过 channel 所属的 eventLoop 调用 poller 的相应方法，注册 fd 的事件
    loop_->updateChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    std::shared_ptr<void> guard;
    if (tied_) {
        guard = tie_.lock();
        if (guard) { handleEventWithGuard(receiveTime); }
    }
    else {
        handleEventWithGuard(receiveTime);
    }
}


void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    if (static_cast<bool>(revents_ & EPOLLHUP) && !static_cast<bool>(revents_ & EPOLLIN)) {
        if (closeCallback_) { closeCallback_(); }
    }

    if (static_cast<bool>(revents_ & EPOLLERR)) {
        if (errorCallback_) { errorCallback_(); }
    }

    if (static_cast<bool>(revents_ & (EPOLLIN | EPOLLPRI))) {
        if (readCallback_) { readCallback_(receiveTime); }
    }

    if (static_cast<bool>(revents_ & EPOLLOUT)) {
        if (writeCallback_) { writeCallback_(); }
    }
}