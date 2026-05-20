#include "TcpServer.h"
#include "Acceptor.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "Logger.h"
#include "TcpConnection.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

using namespace mymuduo;

namespace {
EventLoop* checkNotNull(EventLoop* loop)
{
    if (loop == nullptr) {
        LOG_FATAL << "TcpServer loop is nullptr";
    }
    return loop;
}
}   // namespace

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr, std::string nameArg, Option option)
    : loop_(checkNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(std::move(nameArg))
    , acceptor_(std::make_unique<Acceptor>(loop, listenAddr, option == KReusePort))
    , thread_pool_(std::make_shared<EventLoopThreadPool>(loop, name_))
    , connectionCallback_(defaultConnectionCallback)
    , messageCallback_(defaultMessageCallback)
    , started_(0)
    , nextConnId_(1)
{
    acceptor_->setNewConnectionCallback(
        [this](int sockfd, const InetAddress& peerAddr) { newConnection(sockfd, peerAddr); });
}

TcpServer::~TcpServer()
{
    for (auto& item : connections_) {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->getLoop()->runInLoop([conn = std::move(conn)] { conn->connectDestroyed(); });
    }
}

void TcpServer::setThreadNum(int numThreads)
{
    thread_pool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
    // 保证 TcpServer::start() 只真正执行一次
    if (started_.fetch_add(1) == 0) {

        // 启动 EventLoopThreadPool：
        // 如果 numThreads > 0，则创建并启动 numThreads 个 IO 线程，
        // 每个 IO 线程里运行一个 EventLoop，也就是 subLoop。
        thread_pool_->start(threadInitCallback_);

        // 在 baseLoop 所属线程中执行 Acceptor::listen()
        // listen() 会让监听 socket 开始 listen，并把 acceptChannel 注册到 baseLoop 的 Poller 中，
        // 后续新连接事件由 baseLoop 负责处理。
        loop_->runInLoop([this] { acceptor_->listen(); });
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    auto* ioLoop = thread_pool_->getNextLoop();
    char buf[64];
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_++);
    std::string connName = name_ + buf;

    LOG_INFO << "new connection server=" << name_.c_str() << " conn=" << connName.c_str() << " fd=" << sockfd << " peer=" << peerAddr.toIpPort().c_str();

    // 通过 sockfd 获取其绑定的本机的 ip 地址和端口
    sockaddr_in local;
    socklen_t addrlen = sizeof(local);
    if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&local), &addrlen) < 0) {
        LOG_ERROR << "getsockname failed fd=" << sockfd << " errno=" << errno << " error=" << strerror(errno);
    }

    InetAddress localAddr(local);

    // 创建 TcpConnection 对象
    TcpConnectionPtr conn(std::make_shared<TcpConnection>(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_.emplace(connName, conn);

    // 用户设置给 TcpServer 的回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback([this](const TcpConnectionPtr& conn) { removeConnection(conn); });
    ioLoop->runInLoop([conn] { conn->connectEstablished(); });
}

void TcpServer::removeConnection(TcpConnectionPtr conn)
{
    loop_->runInLoop([this, conn = std::move(conn)] { removeConnectionInLoop(conn); });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    LOG_DEBUG << "remove connection server=" << name_.c_str() << " conn=" << conn->name().c_str();

    connections_.erase(conn->name());
    auto* ioLoop = conn->getLoop();
    ioLoop->queueInLoop([conn] { conn->connectDestroyed(); });
}