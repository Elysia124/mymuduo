#include "Socket.h"
#include "InetAddress.h"
#include "Logger.h"
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace mymuduo;

namespace {
void setNonBlockAndCloseOnExec(int connfd)
{
    int optVal = 1;
    socklen_t optlen = sizeof(optVal);
    if (::setsockopt(connfd, SOL_SOCKET, SOCK_NONBLOCK | SOCK_CLOEXEC, &optVal, optlen) > 0) {
        LOG_FATAL("%s%s%d setNonBlockAndCloseOnExec fail: %s\n", __FILE__, __FUNCTION__, __LINE__, strerror(errno));
    };
}

}   // namespace

Socket::~Socket()
{
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress& localaddr)
{
    if (::bind(sockfd_,
               reinterpret_cast<sockaddr*>(const_cast<sockaddr_in*>(localaddr.getSockAddr())),
               sizeof(sockaddr_in)) < 0) {
        LOG_FATAL("bind sockfd %d fail\n", sockfd_);
    }
}

void Socket::listen() const
{
    if (::listen(sockfd_, SOMAXCONN) < 0) {
        LOG_FATAL("listen sockfd %d fail\n", sockfd_);
    }
}

int Socket::accept(InetAddress* peeraddr) const
{
    sockaddr_in addr;
    socklen_t len = sizeof(addr);

    int connfd = ::accept(sockfd_, reinterpret_cast<sockaddr*>(&addr), &len);
    setNonBlockAndCloseOnExec(connfd);
    
    if (connfd >= 0) {
        peeraddr->setSockAddr(addr);
    }

    return connfd;
}

void Socket::shutdownWrite() const
{
    if (::shutdown(sockfd_, SHUT_WR) < 0) {
        LOG_ERROR("shutdownWrite error\n");
    }
}

void Socket::setTcpNoDelay(bool on) const
{
    int optVal = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optVal, sizeof(optVal));
}

void Socket::setReuseAddr(bool on) const
{
    int optVal = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal));
}

void Socket::setReusePort(bool on) const
{
    int optVal = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optVal, sizeof(optVal));
}

void Socket::setKeepAlive(bool on) const
{
    int optVal = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optVal, sizeof(optVal));
}