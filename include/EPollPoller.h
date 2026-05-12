#pragma once

#include "Poller.h"
#include "Timestamp.h"
#include <sys/epoll.h>
#include <vector>
namespace mymuduo {
class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop* loop);
    ~EPollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;

    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;


private:
    // 填充活跃连接
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

    // 更新 channel
    void update(int operation, Channel* channel) const;


    static const int kInitEventListSize = 16;
    using EventList = std::vector<epoll_event>;

    EventList events_;   // epoll_wait 返回的已经发生且准备就绪的事件列表
    int epollfd_;
};
}   // namespace mymuduo