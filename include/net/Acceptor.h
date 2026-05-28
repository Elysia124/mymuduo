#pragma once

#include "net/Channel.h"
#include "net/Socket.h"
#include "net/noncopyable.h"
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

    bool listening() const { return listening_; }
    void listen();

private:
    void handleRead();

    EventLoop* loop_;   // Acceptor uses baseloop(mianloop)
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_;
};
}   // namespace mymuduo