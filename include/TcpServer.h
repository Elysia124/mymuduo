#pragma once

#include "Acceptor.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

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

    TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option option = kNoReusePort);
    ~TcpServer();

    void setThreadNum(int numThreads);
    void start();

    void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; };
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; };
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; };
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; };

private:
    void newConnection(int sockfd, const InetAddress& peeraddr);
    void removeConnecttion(const TcpConnectionPtr& conn);
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
    ConnectionMap connextMap_;
};
}   // namespace mymuduo