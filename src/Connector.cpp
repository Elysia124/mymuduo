#include "Connector.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include <algorithm>
#include <asm-generic/socket.h>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using namespace mymuduo;

namespace {
bool isSelfConnect(int sockfd)
{
    sockaddr_in localAddr;
    socklen_t localAddrlen = sizeof(localAddr);
    if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&localAddr), &localAddrlen) < 0) {
        LOG_FATAL << "getsockname fail errno=" << errno << ", error=" << strerror(errno);
    }

    sockaddr_in peerAddr;
    socklen_t peerAddrlen = sizeof(peerAddr);
    if (::getpeername(sockfd, reinterpret_cast<sockaddr*>(&peerAddr), &peerAddrlen) < 0) {
        LOG_FATAL << "getsockname fail errno=" << errno << ", error=" << strerror(errno);
    }

    return localAddr.sin_port == peerAddr.sin_port && localAddr.sin_addr.s_addr == peerAddr.sin_addr.s_addr;
}
}   // namespace

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop)
    , serverAddr_(serverAddr)
    , connect_(false)
    , state_(State::kDisconnected)
    , retryDelayMs_(kInitRetryDelayMs)
{
    LOG_DEBUG << "Connector created";
}

Connector::~Connector()
{
    LOG_DEBUG << "Connector destroyed";
}

void Connector::start()
{
    connect_.store(true, std::memory_order_relaxed);

    // keep connector alive until startInLoop() has run
    loop_->runInLoop([self = shared_from_this()] { self->startInLoop(); });
}
void Connector::startInLoop()
{
    loop_->assertInLoopThread();
    if (connect_.load(std::memory_order_relaxed)) {
        connect();
    }
    else {
        LOG_DEBUG << "do not connect";
    }
}

void Connector::stop()
{
    connect_.store(false, std::memory_order_relaxed);

    // defer stopInLoop() because it may remove channel
    // this avoids removing a channel while it is still being handled
    loop_->queueInLoop([self = shared_from_this()] { self->stopInLoop(); });
}

void Connector::stopInLoop()
{
    loop_->assertInLoopThread();
    if (state_.load(std::memory_order_relaxed) == State::kConnecting) {
        state_.store(State::kDisconnected, std::memory_order_relaxed);
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

void Connector::connect()
{
    int sockfd = ::socket(AF_INET, SOCK_NONBLOCK | SOCK_STREAM | SOCK_CLOEXEC, 0);   // start connecting
    if (sockfd < 0) {
        LOG_FATAL << "connect socket create failed: errno=" << errno << " error=" << strerror(errno);
    }

    int ret = ::connect(sockfd, reinterpret_cast<const sockaddr*>(serverAddr_.getSockAddr()), sizeof(serverAddr_));
    int savedErrno = ret == 0 ? 0 : errno;

    switch (savedErrno) {
        case 0:
        case EINPROGRESS:   // is connecting now
        case EINTR:
        case EISCONN:
            connecting(sockfd);
            break;

        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            retry(sockfd);
            break;

        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            LOG_ERROR << "connect failed: errno=" << savedErrno << " error=" << strerror(savedErrno);
            ::close(sockfd);
            break;

        default:
            LOG_ERROR << "Unexpected error: errno=" << savedErrno;
            ::close(sockfd);
            break;
    }
}

void Connector::restart()
{
    loop_->assertInLoopThread();
    setState(State::kDisconnected);
    connect_.store(true, std::memory_order_relaxed);
    startInLoop();
}

void Connector::connecting(int sockfd)
{
    setState(State::kConnecting);
    assert(!channel_);
    channel_ = std::make_unique<Channel>(loop_, sockfd);   // 把 loop 和 非阻塞连接的 sockfd 封装成一个 channel
    channel_->setWriteCallback([self = shared_from_this()] { self->handleWrite(); });
    channel_->setErrorCallback([self = shared_from_this()] { self->handleError(); });
    channel_->enableWriting();   // register write event to poller
}

int Connector::removeAndResetChannel()
{   // Connector 取消对 sockfd 的管理，并销毁对应的 chaneel，返回 sockfd (移交给 TcpConnection 管理)
    channel_->disableAll();
    channel_->remove();   // 从 poller 中删除该channel
    int sockfd = channel_->fd();
    loop_->queueInLoop([self = shared_from_this()] { self->resetChannel(); });
    return sockfd;
}

void Connector::resetChannel()
{
    channel_.reset();
}


void Connector::handleWrite()
{
    if (state_.load(std::memory_order_relaxed) == State::kConnecting) {
        int sockfd = removeAndResetChannel();
        int optVal = 0;
        socklen_t optlen = sizeof(optVal);

        // sockfd 可读不代表连接成功，需调用 getsockopt 拿到 具体的 SO_ERROR 值来判断
        if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optVal, &optlen) < 0) {
            // getsockopt 本身调用失败
            LOG_ERROR << "getsockopt failed errno=" << errno << ", error: " << strerror(errno);
            retry(sockfd);
        }
        else if (optVal != 0) {
            LOG_WARN << "connect failed errno=" << optVal << ", error=" << strerror(optVal);
            retry(sockfd);
        }
        else if (isSelfConnect(sockfd)) {   // 自连接
            LOG_WARN << "self connect";
            retry(sockfd);
        }
        else {   // connect success
            setState(State::kConnected);
            if (connect_.load(std::memory_order_relaxed)) {
                newConnectionCallback(sockfd);
            }
            else {
                ::close(sockfd);
            }
        }
    }
}

void Connector::handleError()
{
    if (state_.load(std::memory_order_relaxed) == State::kConnecting) {
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

void Connector::retry(int sockfd)
{
    ::close(sockfd);
    setState(State::kDisconnected);
    if (connect_.load(std::memory_order_relaxed)) {
        LOG_INFO << "Retry connecting to " << serverAddr_.toIpPort().c_str() << " in " << retryDelayMs_
                 << " milliseconds";
        loop_->runAfter(retryDelayMs_ / 1000.0, [self = shared_from_this()] { self->startInLoop(); });
        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
    }
    else {
        LOG_DEBUG << "do not connect";
    }
}
