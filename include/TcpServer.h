#pragma once

#include "Acceptor.h"
#include "Buffer.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "noncopyable.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace mymuduo {
class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option
    {
        kNoReusePort,
        KReusePort
    };

    TcpServer(EventLoop* loop, const InetAddress& listenAddr, std::string nameArg, Option option = kNoReusePort);
    ~TcpServer();

    void setThreadNum(int numThreads);
    void start();

    void setThreadInitCallback(ThreadInitCallback cb) { threadInitCallback_ = std::move(cb); };
    void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); };
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); };
    void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); };

private:
    void newConnection(int sockfd, const InetAddress& peeraddr);
    void removeConnection(TcpConnectionPtr conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop* loop_;   // the acceptor loop(base loop)
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_;   // 运行在 baseloop
    std::shared_ptr<EventLoopThreadPool> thread_pool_;
    ConnectionCallback connectionCallback_;         // 有新连接时的回调
    MessageCallback messageCallback_;               // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;   // 消息发送完成以后的回调
    ThreadInitCallback threadInitCallback_;         // loop线程初始化的回调
    std::atomic<int> started_;

    int nextConnId_;
    ConnectionMap connections_;
};
}   // namespace mymuduo