#include "net/TcpConnection.h"
#include "net/Callbacks.h"
#include "net/Channel.h"
#include "net/EventLoop.h"
#include "net/Logger.h"
#include "net/Socket.h"
#include "net/Timestamp.h"
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
using namespace mymuduo;

namespace {
EventLoop* checkNotNull(EventLoop* loop)
{
    if (loop == nullptr) {
        LOG_FATAL << "TcpConnection loop is nullptr";
    }
    return loop;
}
}   // namespace


TcpConnection::TcpConnection(EventLoop* loop, std::string nameArg, int sockfd, const InetAddress& localAddr,
                             const InetAddress& peerAddr)
    : loop_(checkNotNull(loop))
    , name_(std::move(nameArg))
    , state_(StateE::kConnecting)
    , reading_(true)
    , socket_(std::make_unique<Socket>(sockfd))
    , channel_(std::make_unique<Channel>(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(static_cast<ssize_t>(64 * 1024 * 1024))
{
    channel_->setReadCallback([this](Timestamp ts) { handleRead(ts); });
    channel_->setWriteCallback([this] { handleWrite(); });
    channel_->setCloseCallback([this] { handleClose(); });
    channel_->setErrorCallback([this] { handleError(); });

    LOG_DEBUG << "TcpConnection created name=" << name_.c_str() << " fd=" << sockfd
              << " local=" << localAddr_.toIpPort().c_str() << " peer=" << peerAddr_.toIpPort().c_str();
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_DEBUG << "TcpConnection destroyed name=" << name_.c_str() << " fd=" << channel_->fd()
              << " state=" << static_cast<int>(state_.load(std::memory_order_relaxed));
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int saveErrno = 0;
    auto n = inputBuffer_.readFd(channel_->fd(), saveErrno);
    if (n > 0) {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0) {
        handleClose();
    }
    else {
        if (saveErrno == EAGAIN || saveErrno == EWOULDBLOCK) {
            return;
        }
        errno = saveErrno;
        LOG_ERROR << "read failed name=" << name_.c_str() << " fd=" << channel_->fd()
                  << " errno=" << saveErrno << " error=" << strerror(saveErrno);
        handleError();
        handleClose();
    }
}

void TcpConnection::handleWrite()
{
    if (channel_->isWriting()) {
        int saveErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), saveErrno);
        if (n > 0) {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0) {

                // 写完全部数据后，关闭可写事件的监听
                // 因为 socket 大部分事件都可写，所以按需开启
                channel_->disableWriting();
                if (writeCompleteCallback_) {

                    // 这里是为了避免 writeCompleteCallback_ 中含有其他操作可能导致重入 TcpConnection 逻辑
                    auto self = shared_from_this();
                    loop_->queueInLoop([self] { self->writeCompleteCallback_(self); });
                }
                if (state_.load(std::memory_order_relaxed) == StateE::kdisconnecting) {
                    shutdownInLoop();
                }
            }
        }
        else if (saveErrno != EAGAIN && saveErrno != EWOULDBLOCK) {
            LOG_ERROR << "write failed name=" << name_.c_str() << " fd=" << channel_->fd()
                      << " errno=" << saveErrno << " error=" << strerror(saveErrno);
            if (saveErrno == EPIPE || saveErrno == ECONNRESET) {
                handleClose();
            }
        }
    }
    else {   // 当前没有关注可写事件却收到了写回调，通常是状态变化或过期事件导致的。
        LOG_WARN << "unexpected write event on non-writing channel name=" << name_.c_str()
                 << " fd=" << channel_->fd()
                 << " state=" << static_cast<int>(state_.load(std::memory_order_relaxed));
    }
}

void TcpConnection::handleClose()
{
    StateE oldState = state_.exchange(StateE::kdisconnected, std::memory_order_relaxed);

    if (oldState == StateE::kdisconnected) {
        return;
    }

    LOG_INFO << "connection closed name=" << name_.c_str() << " fd=" << channel_->fd()
             << " state=" << static_cast<int>(state_.load(std::memory_order_relaxed))
             << " peer=" << peerAddr_.toIpPort().c_str();
    setState(StateE::kdisconnected);
    channel_->disableAll();

    TcpConnectionPtr guardThis(shared_from_this());
    connectionCallback_(guardThis);
    closeCallback_(guardThis);
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int saveErrno;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        saveErrno = errno;
    }
    else {
        saveErrno = optval;
    }

    if (saveErrno == ECONNRESET || saveErrno == EPIPE) {
        LOG_DEBUG << "socket closed by peer name=" << name_.c_str() << " fd=" << channel_->fd()
                  << " so_error=" << saveErrno << " error=" << strerror(saveErrno);
        return;
    }

    LOG_ERROR << "socket error name=" << name_.c_str() << " fd=" << channel_->fd()
              << " so_error=" << saveErrno << " error=" << strerror(saveErrno);
}

void TcpConnection::send(std::string_view buf)
{
    if (state_.load(std::memory_order_relaxed) == StateE::kconnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(buf.data(), buf.size());
        }
        else {
            send(std::string(buf));   // call send(std::string&& buf)
        }
    }
}

void TcpConnection::send(std::string&& buf)
{
    if (state_.load(std::memory_order_relaxed) == StateE::kconnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(buf.c_str(), buf.size());
        }
        else {
            auto self = shared_from_this();

            // runInLoop会判断是否在自己所属的线程
            loop_->runInLoop([self, msg = std::move(buf)]() { self->sendInLoop(msg.c_str(), msg.size()); });
        }
    }
}

void TcpConnection::send(const char* start, std::size_t len)
{
    if (state_.load(std::memory_order_relaxed) != StateE::kconnected) {
        return;
    }

    if (loop_->isInLoopThread()) {
        sendInLoop(start, len);
    }
    else {
        send(std::string(start, len));
    }
}

void TcpConnection::send(Buffer* buffer)
{
    if (state_.load(std::memory_order_relaxed) != StateE::kconnected) {
        buffer->retrieveAll();
        return;
    }

    if (loop_->isInLoopThread()) {
        sendInLoop(buffer->peek(), buffer->readableBytes());
        buffer->retrieveAll();
    }
    else {
        send(buffer->retrieveAllAsString());
    }
}

void TcpConnection::sendInLoop(const void* message, std::size_t len)
{
    ssize_t nwrote = 0;
    std::size_t remaining = len;
    bool faultError = false;

    if (state_.load(std::memory_order_relaxed) == StateE::kdisconnected) {
        LOG_WARN << "send ignored because connection is disconnected name=" << name_.c_str()
                 << " fd=" << channel_->fd();
        return;
    }

    // channel第一次开始写数据且缓冲区没有待发送数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->fd(), message, len);
        if (nwrote >= 0) {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_) {
                loop_->queueInLoop([self = shared_from_this()] { self->writeCompleteCallback_(self); });
            }
        }
        else {
            nwrote = 0;
            if (errno != EAGAIN) {
                LOG_ERROR << "write failed while sending name=" << name_.c_str() << " fd=" << channel_->fd()
                          << " errno=" << errno << " error=" << strerror(errno);
            }
            if (errno == EPIPE || errno == ECONNRESET) {
                faultError = true;
            }
        }
    }

    // 数据未一次性写完
    if (!faultError && remaining > 0) {
        std::size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_) {
            loop_->queueInLoop([self = shared_from_this(), len = oldLen + remaining] {
                self->highWaterMarkCallback_(self, len);
            });
        }

        outputBuffer_.append(static_cast<const char*>(message) + nwrote, remaining);
        if (!channel_->isWriting()) {
            // 注册写感兴趣事件
            channel_->enableWriting();
        }
    }
}

// 连接建立
void TcpConnection::connectEstablished()
{
    setState(StateE::kconnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    connectionCallback_(shared_from_this());
}

// 连接关闭
void TcpConnection::connectDestroyed()
{
    if (state_.load(std::memory_order_relaxed) == StateE::kconnected) {
        setState(StateE::kdisconnected);
        channel_->disableAll();

        connectionCallback_(shared_from_this());
    }

    channel_->remove();   // 把 channel 从 poller中删除
}

void TcpConnection::forceClose()
{
    StateE expected = state_.load(std::memory_order_relaxed);
    while (expected == StateE::kconnected || expected == StateE::kdisconnecting) {
        if (state_.compare_exchange_weak(
                expected, StateE::kdisconnecting, std::memory_order_relaxed, std::memory_order_relaxed)) {
            loop_->queueInLoop([self = shared_from_this()] { self->forceCloseInLoop(); });
            break;
        }
    }
}

void TcpConnection::forceCloseInLoop()
{
    loop_->assertInLoopThread();
    StateE state = state_.load(std::memory_order_relaxed);
    if (state == StateE::kconnected || state == StateE::kdisconnecting) {
        handleClose();
    }
}

void TcpConnection::shutdown()
{
    if (state_.load(std::memory_order_relaxed) == StateE::kconnected) {
        setState(StateE::kdisconnecting);
        loop_->runInLoop([self = shared_from_this()] { self->shutdownInLoop(); });
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) {   // 说明当前outputBuffer中的数据发送完了
        socket_->shutdownWrite();
    }
}
