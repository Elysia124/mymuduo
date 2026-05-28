#pragma once

#include "net/Buffer.h"
#include "net/Callbacks.h"
#include "net/InetAddress.h"
#include "net/Timestamp.h"
#include "net/noncopyable.h"
#include <any>
#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace mymuduo {
class Channel;
class EventLoop;
class Socket;

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop, std::string nameArg, int sockfd, const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }
    bool connected() const { return state_.load(std::memory_order_relaxed) == StateE::kconnected; }

    void send(std::string_view buf);
    void send(const char* buf) { send(std::string_view(buf)); }
    void send(std::string&& buf);   // for rval
    void send(const char* start, std::size_t len);
    void send(Buffer* buffer);
    void shutdown();

    void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }
    void setHighWaterMarkCallback(HighWaterMarkCallback cb) { highWaterMarkCallback_ = std::move(cb); }

    void connectEstablished();

    void connectDestroyed();

    void forceClose();

    void setContext(std::any context) { context_ = std::move(context); }
    const std::any& getContext() const { return context_; }
    std::any& getMutableContext() { return context_; }

private:
    enum class StateE
    {
        kdisconnected,
        kConnecting,
        kconnected,
        kdisconnecting
    };

    void setState(StateE state) { state_.store(state, std::memory_order_relaxed); }
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, std::size_t len);
    void shutdownInLoop();

    void forceCloseInLoop();

    EventLoop* loop_;   // it's subloop, not baseloop
    const std::string name_;
    std::atomic<StateE> state_;
    bool reading_;
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;         // 有新连接时的回调
    MessageCallback messageCallback_;               // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;   // 消息发送完成以后的回调
    CloseCallback closeCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;   // 流量控制，当发送缓冲区积压数据超过阈值时调用

    std::size_t highWaterMark_;
    Buffer inputBuffer_;
    Buffer outputBuffer_;

    std::any context_;   // Http Context
};
}   // namespace mymuduo