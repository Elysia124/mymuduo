#include "net/Poller.h"
#include "net/Channel.h"
#include "net/EventLoop.h"

using namespace mymuduo;

Poller::Poller(EventLoop* loop) : ownerLoop_(loop) {}

Poller::~Poller() = default;

bool Poller::hasChannel(Channel* channel) const
{
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}