#include "TcpServer.h"
#include "Acceptor.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "Logger.h"

using namespace mymuduo;

namespace {
EventLoop* checkNotNull(EventLoop* loop)
{
    if (loop == nullptr) { LOG_FATAL("%s%s%d baseloop is nullptr! \n", __FILE__, __FUNCTION__, __LINE__); }
    return loop;
}
}   // namespace

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option option)
    : loop_(checkNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == KReusePort))
    , thread_pool_(new EventLoopThreadPool(loop, nameArg))
    , connectionCallback_(defaultConnectionCallback)
    , messageCallback_(defaultMessageCallback)
    , started_(0)
    , nextConnId_(1)
{
    acceptor_->setNewConnectionCallback(
        [this](int sockfd, const InetAddress& peerAddr) { newConnection(sockfd, peerAddr); });
}

TcpServer::~TcpServer() {}

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