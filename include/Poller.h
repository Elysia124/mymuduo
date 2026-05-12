#pragma once

// muduo 库中多路事件分发器的核心


#include "Timestamp.h"
#include "noncopyable.h"
#include <unordered_map>
#include <vector>


namespace mymuduo {
class Channel;
class EventLoop;

class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller();

    // 给所有 IO复用保留同一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;
    virtual bool hasChannel(Channel* channel) const;

    // 获取 EventLoop 的默认 Poller 对象
    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    // key = sockfd, value = sockfd 所属的 channel
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private:
    EventLoop* ownerLoop_;   // Poller 所属的 EventLoop
};
}   // namespace mymuduo