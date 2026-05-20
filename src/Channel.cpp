#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include <memory>
#include <sstream>

using namespace mymuduo;

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
void Channel::update()
{
    // 通过 channel 所属的 eventLoop 调用 poller 的相应方法，注册 fd 的事件
    loop_->updateChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    std::shared_ptr<void> guard;
    if (tied_) {
        guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
    }
    else {
        handleEventWithGuard(receiveTime);
    }
}


void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_TRACE << "channel event fd=" << fd_ << " revents=" << reventsToString().c_str();

    if (static_cast<bool>(revents_ & EPOLLHUP) && !static_cast<bool>(revents_ & EPOLLIN)) {
        if (closeCallback_) {
            closeCallback_();
        }
    }

    if (static_cast<bool>(revents_ & EPOLLERR)) {
        if (errorCallback_) {
            errorCallback_();
        }
    }

    if (static_cast<bool>(revents_ & (EPOLLIN | EPOLLPRI))) {
        if (readCallback_) {
            readCallback_(receiveTime);
        }
    }

    if (static_cast<bool>(revents_ & EPOLLOUT)) {
        if (writeCallback_) {
            writeCallback_();
        }
    }
}

std::string Channel::eventsToString() const
{
    std::ostringstream oss;
    if (events_ == kNoneEvent) {
        oss << "NoneEvent";
        return oss.str();
    }

    if (events_ & EPOLLIN) {
        oss << "EPOLLIN ";
    }
    if (events_ & EPOLLPRI) {
        oss << "EPOLLPRI ";
    }
    if (events_ & EPOLLOUT) {
        oss << "EPOLLOUT ";
    }

    return oss.str();
}

std::string Channel::reventsToString() const
{
    std::ostringstream oss;
    if (revents_ & EPOLLHUP) {
        oss << "EPOLLHUP ";
    }
    if (revents_ & EPOLLERR) {
        oss << "EPOLLERR ";
    }
    if (revents_ & EPOLLIN) {
        oss << "EPOLLIN ";
    }
    if (revents_ & EPOLLPRI) {
        oss << "EPOLLPRI ";
    }
    if (revents_ & EPOLLOUT) {
        oss << "EPOLLOUT ";
    }


    return oss.str();
}