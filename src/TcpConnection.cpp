#include "TcpConnection.h"
#include "Callbacks.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Socket.h"
#include "Timestamp.h"
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
using namespace mymuduo;

namespace {
EventLoop* checkNotNull(EventLoop* loop)
{
    if (loop == nullptr) {
        LOG_FATAL("%s%s%d Tcpconnection loop is nullptr! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}
}   // namespace

TcpConnection::TcpConnection(EventLoop* loop, std::string nameArg, int sockfd, const InetAddress& localAddr,
                             const InetAddress& peerAddr)
    : loop_(checkNotNull(loop))
    , name_(std::move(nameArg))
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64 * 1024 * 1024)
{
    channel_->setReadCallback([this](Timestamp ts) { handleRead(ts); });
    channel_->setWriteCallback([this] { handleWrite(); });
    channel_->setCloseallback([this] { handleClose(); });
    channel_->seterrorallback([this] { handleError(); });

    LOG_DEBUG("TcpConnection::ctor[%s] at fd = %d\n", name_.c_str(), fd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_DEBUG("TcpConnection::dtor[%s] at fd = %d state=%d\n", name_.c_str(), channel_->fd(), state_.load());
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
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead\n");
        handleError();
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
                if (state_.load() == kDisconnecting) {
                    shutdown();
                }
            }
        }
        else {
            LOG_ERROR("TcpConnection::handleWrite\n");
        }
    }
    else {   // 当前没有关注可写事件却收到了写回调，通常是状态变化或过期事件导致的。
        LOG_ERROR("TcpConnection::handleWrite Connection fd = %d is down, no more writing\n", channel_->fd());
    }
}

void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd = %d state = %d\n", channel_->fd(), state_.load());
    setState(kDisconnected);
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

    LOG_ERROR("TcpConnection::handleError name: %s - SO_ERROR = %s", name_.c_str(), strerror(saveErrno));
}

void TcpConnection::send(const std::string& buf)
{
    if (state_.load() == kconnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(buf.c_str(), buf.size());
        }
        else {
            auto self = shared_from_this();

            // runInLoop会判断是否在自己所属的线程
            loop_->runInLoop([&, self]() { sendInLoop(buf.c_str(), buf.size()); });
        }
    }
}

void TcpConnection::sendInLoop(const void* message, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if (state_.load() == kDisconnected) {
        LOG_ERROR("TcpConnection::sendInLoop disconnected, give up writing\n");
        return;
    }

    // channel第一次开始写数据且缓冲区没有待发送数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->fd(), message, len);
        if (nwrote >= 0) {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_) {
                auto self = shared_from_this();
                loop_->queueInLoop([self] { self->writeCompleteCallback_(self); });
            }
        }
        else {
            nwrote = 0;
            if (errno != EAGAIN) {
                LOG_ERROR("TcpConnection::sendInLoop\n");
            }
            if (errno == EPIPE || errno == ECONNRESET) {
                faultError = true;
            }
        }
    }

    // 数据未一次性写完
    if (!faultError && remaining > 0) {
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_) {
            auto self = shared_from_this();
            loop_->queueInLoop([&, self] { self->highWaterMarkCallback_(self, oldLen + remaining); });
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
    setState(kconnected);
    channel_->tie(shared_from_this());
    channel_->enabelReading();

    connectionCallback_(shared_from_this());
}

// 连接关闭
void TcpConnection::connectDestory()
{
    if (state_.load() == kconnected) {
        setState(kDisconnected);
        channel_->disableAll();

        ConnectionCallback(shared_from_this());
    }

    channel_->remove();   // 把 channel 从 poller中删除
}

void TcpConnection::shutdown()
{
    if (state_.load() == kconnected) {
        setState(kDisconnecting);
        loop_->runInLoop([this] { shutdownInLoop(); });
    }
}

void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) {   // 说明当前outputBuffer中的数据发送完了
        socket_->shutdownWrite();
    }
}