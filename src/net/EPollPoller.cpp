#include "net/EPollPoller.h"
#include "net/Channel.h"
#include "net/Logger.h"
#include <cerrno>
#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>


using namespace mymuduo;

namespace {
const int kNew = -1;      // channel 未添加到 poller 和 channels_ 中
const int kAdded = 1;     // channel 已添加到 poller 和 channels_ 中
const int kDeleted = 2;   // channel 已从 poller 中删除，但 channels_ 中仍有

}   // namespace

EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop), events_(kInitEventListSize), epollfd_(::epoll_create1(EPOLL_CLOEXEC))
{
    if (epollfd_ < 0) {
        LOG_FATAL << "epoll_create1 failed: errno=" << errno << " error=" << strerror(errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    LOG_TRACE << "epoll_wait start timeout_ms=" << timeoutMs << " channel_count=" << channels_.size();

    int numEvents = epoll_wait(epollfd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);
    int saveError = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0) {
        LOG_TRACE << "epoll_wait returned events=" << numEvents;
        fillActiveChannels(numEvents, activeChannels);

        if (numEvents == static_cast<int>(events_.size())) {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0) {
        LOG_TRACE << "epoll_wait timeout timeout_ms=" << timeoutMs;
    }
    else {
        if (saveError != EINTR) {
            LOG_ERROR << "epoll_wait failed: errno=" << saveError << " error=" << strerror(saveError);
            errno = saveError;
        }
    }

    return now;
}

void EPollPoller::updateChannel(Channel* channel)
{
    const int index = channel->index();
    LOG_TRACE << "update channel fd=" << channel->fd() << " events=" << channel->eventsToString().c_str() << " index=" << channel->index();

    if (index == kNew || index == kDeleted) {
        if (index == kNew) {
            channels_[channel->fd()] = channel;
        }

        channel->setIndex(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else {   // index == kAdded
        if (channel->isNoneEvent()) {
            // 如果 channel 不关注任何事件，则表示删除
            update(EPOLL_CTL_DEL, channel);
            channel->setIndex(kDeleted);
        }
        else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// channel->remove => eventLoop->removeChannel => poller->removeChannel
void EPollPoller::removeChannel(Channel* channel)
{
    LOG_TRACE << "remove channel fd=" << channel->fd() << " index=" << channel->index();

    int fd = channel->fd();
    int index = channel->index();

    if (index == kAdded) {   // 从 poller 中删除 channel
        update(EPOLL_CTL_DEL, channel);
    }

    // 从 channels 中删除 channel
    channels_.erase(fd);
    channel->setIndex(kNew);
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
    for (int i = 0; i < numEvents; ++i) {
        auto* channel = static_cast<Channel*>(events_[i].data.ptr);

        channel->setRevents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

void EPollPoller::update(int operation, Channel* channel) const
{
    epoll_event event{};
    event.events = channel->events();
    event.data.ptr = channel;
    int fd = channel->fd();

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            LOG_ERROR << "epoll_ctl DEL failed fd=" << fd << ": errno=" << errno << " error=" << strerror(errno);
        }
        else {
            LOG_FATAL << "epoll_ctl ADD/MOD failed fd=" << fd << " op=" << operation << " events=" << event.events << ": errno=" << errno << " error=" << strerror(errno);
        }
    }
}