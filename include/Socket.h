#pragma once

#include "noncopyable.h"
namespace mymuduo {

class InetAddress;
// 封装 socketfd
class Socket : noncopyable
{
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    ~Socket();

    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress& localaddr) const;
    void listen() const;
    int accept(InetAddress* peeraddr) const;
    void shutdownWrite() const;

    void setTcpNoDelay(bool on) const;
    void setReuseAddr(bool on) const;
    void setReusePort(bool on) const;
    void setKeepAlive(bool on) const;

private:
    const int sockfd_;
};
}   // namespace mymuduo