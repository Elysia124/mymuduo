#pragma once

#include "Channel.h"
#include "Socket.h"
#include "noncopyable.h"
#include <functional>

namespace mymuduo {
class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int Socket, const InetAddress&)>;
    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(NewConnectionCallback cb) { newConnectionCallback_ = std::move(cb); }

    bool listenning() const { return listenning_; }
    void listen();

private:
    void handelRead();

    EventLoop* loop_;   // Acceptor uses baseloop(mianloop)
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};
}   // namespace mymuduo