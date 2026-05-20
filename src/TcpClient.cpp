#include "TcpClient.h"
#include "Callbacks.h"
#include "Connector.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Logger.h"
#include "Poller.h"
#include "TcpConnection.h"
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

using namespace mymuduo;

namespace {
EventLoop* checkNotNull(EventLoop* loop)
{
    if (loop == nullptr) {
        LOG_FATAL << "TcpClient loop is nullptr";
    }
    return loop;
}
}   // namespace

namespace mymuduo::detail {
void removeConnection(EventLoop* loop, const TcpConnectionPtr& conn)
{
    loop->queueInLoop([conn] { conn->connectDestroyed(); });
}

void removeConnector(const ConnectorPtr&) {}
}   // namespace mymuduo::detail

TcpClient::TcpClient(EventLoop* loop, const InetAddress& serverAddr, std::string nameArg)
    : loop_(checkNotNull(loop))
    , connector_(std::make_shared<Connector>(loop, serverAddr))
    , name_(std::move(nameArg))
    , connectionCallback_(defaultConnectionCallback)
    , messageCallback_(defaultMessageCallback)
    , retry_(false)
    , connect_(true)
    , nextConnId_(1)
{
    connector_->setNewConnectionCallback([this](int sockfd) { newConnection(sockfd); });
    LOG_DEBUG << "TcpClient[" << name_.c_str() << "] - connector " << static_cast<void*>(get_pointer(connector_))
              << " created";
}

TcpClient::~TcpClient()
{
    LOG_DEBUG << "TcpClient[" << name_.c_str() << "] - connector " << static_cast<void*>(get_pointer(connector_))
              << " destroyed";
    TcpConnectionPtr conn;
    bool unique = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        unique = connection_.unique();
        conn = connection_;
    }

    if (conn) {
        CloseCallback closeCallback = [loop = loop_](const TcpConnectionPtr& conn) {
            detail::removeConnection(loop, conn);
        };
        loop_->runInLoop([conn, cb = std::move(closeCallback)]() mutable { conn->setCloseCallback(std::move(cb)); });
        if (unique) {
            conn->forceClose();
        }
    }
    else {
        connector_->stop();
        loop_->runAfter(1.0, [connector = connector_]() { detail::removeConnector(connector); });
    }
}

void TcpClient::connect()
{
    LOG_INFO << "TcpClient[" << name_.c_str() << "] is connecting to "
             << connector_->serverAddress().toIpPort().c_str();
    connect_.store(true, std::memory_order_relaxed);
    connector_->start();
}

void TcpClient::disconnect()
{
    connect_.store(false, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connection_) {
            connection_->shutdown();
        }
    }
}

void TcpClient::stop()
{
    connect_.store(false, std::memory_order_relaxed);
    connector_->stop();
}

void TcpClient::newConnection(int sockfd)
{
    loop_->assertInLoopThread();
    sockaddr_in peerSockAddr;
    socklen_t peerSockAddrlen = sizeof(peerSockAddr);
    if (::getpeername(sockfd, reinterpret_cast<sockaddr*>(&peerSockAddr), &peerSockAddrlen) < 0) {
        LOG_FATAL << "get peerAddr fail, errno=" << errno << ", error=" << strerror(errno);
    }
    InetAddress peerAddr(peerSockAddr);

    char buf[32]{};
    snprintf(buf, sizeof(buf), "%s#%d", peerAddr.toIpPort().c_str(), nextConnId_++);
    std::string connName = name_ + buf;

    sockaddr_in localSockAddr;
    socklen_t localSockAddrlen = sizeof(localSockAddr);
    if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&localSockAddr), &localSockAddrlen) < 0) {
        LOG_FATAL << "get localAddr fail, errno=" << errno << ", error=" << strerror(errno);
    }
    InetAddress localAddr(localSockAddr);

    auto conn = std::make_shared<TcpConnection>(loop_, connName, sockfd, localAddr, peerAddr);

    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback([this](const TcpConnectionPtr& conn) { removeConnection(conn); });

    {
        std::lock_guard<std::mutex> lock(mutex_);
        connection_ = conn;
    }

    conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->assertInLoopThread();
    assert(loop_ == conn->getLoop());

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (conn == connection_) {
            connection_.reset();
        }
    }

    loop_->queueInLoop([conn] { conn->connectDestroyed(); });
    if (retry_ && connect_) {
        LOG_INFO << "TcpClient[" << name_.c_str() << "] reconnecting to "
                 << connector_->serverAddress().toIpPort().c_str();
        connector_->restart();
    }
}
