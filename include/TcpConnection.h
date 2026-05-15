#pragma once

#include "Acceptor.h"
#include "Buffer.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "Timestamp.h"
#include "noncopyable.h"
#include <atomic>
#include <cstddef>
#include <memory>
#include <string>

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
    const InetAddress& localAdderss() const { return localAddr_; }
    const InetAddress& peerAdderss() const { return peerAddr_; }
    bool connected() const { return state_ == StateE::kconnected; }

    void send(const std::string& buf);
    void shutdown();

    void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }
    void setHighWaterMarkCallback(HighWaterMarkCallback cb) { highWaterMarkCallback_ = std::move(cb); }

    void connectEstablished();

    void connectDestroy();


private:
    enum class StateE
    {
        kDisconnected,
        kConnecting,
        kconnected,
        kDisconnecting
    };

    void setState(StateE state) { state_.store(state); }
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();

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

    size_t highWaterMark_;
    Buffer inputBuffer_;
    Buffer outputBuffer_;
};
}   // namespace mymuduo