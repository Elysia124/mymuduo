#pragma once

#include "Callbacks.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "noncopyable.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
namespace mymuduo {
class Connector;
using ConnectorPtr = std::shared_ptr<Connector>;

class TcpClient : noncopyable
{
public:
    TcpClient(EventLoop* loop, const InetAddress& serverAddr, std::string nameArg);
    ~TcpClient();

    void connect();
    void disconnect();
    void stop();

    TcpConnectionPtr connection() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_;
    }

    EventLoop* getLoop() const { return loop_; }
    bool retry() const { return retry_.load(std::memory_order_relaxed); }
    void enableRetry() { retry_.store(true, std::memory_order_relaxed); }

    const std::string& name() const { return name_; }

    void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }

private:
    void newConnection(int sockfd);
    void removeConnection(const TcpConnectionPtr& conn);

    EventLoop* loop_;
    ConnectorPtr connector_;
    const std::string name_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    std::atomic<bool> retry_;
    std::atomic<bool> connect_;   // 是否保持连接
    int nextConnId_;
    mutable std::mutex mutex_;
    TcpConnectionPtr connection_;
};
}   // namespace mymuduo