#include "Connector.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include <atomic>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
using namespace mymuduo;

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop)
    , serverAddr_(serverAddr)
    , connect_(false)
    , state_(State::kDisconnected)
    , retryDelayMs_(kInitRetryDelayMs)
{
    LOG_DEBUG("Connector created");
}

Connector::~Connector()
{
    LOG_DEBUG("Connector destroyed");
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
        LOG_DEBUG("Do not connect");
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
    int sockfd = ::socket(AF_INET, SOCK_NONBLOCK | SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (sockfd < 0) {
        LOG_FATAL("connect socket create failed: errno=%d error=%s", errno, strerror(errno));
    }

    int ret = ::connect(sockfd, reinterpret_cast<sockaddr*>(&serverAddr_), sizeof(serverAddr_));
    int savedErrno = ret == 0 ? 0 : errno;

    switch (savedErrno) {
        case 0:
        case EINPROGRESS:
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
            LOG_ERROR("connect failed: errno=%d error=%s", savedErrno, strerror(savedErrno));
            ::close(sockfd);
            break;

        default:
            LOG_ERROR("Unexpected error: errno=%d", savedErrno);
            ::close(sockfd);
            break;
    }
}