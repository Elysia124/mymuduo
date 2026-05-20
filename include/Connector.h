#pragma once

#include "InetAddress.h"
#include "noncopyable.h"
#include <atomic>
#include <functional>
#include <memory>

namespace mymuduo {
class Channel;
class EventLoop;

class Connector : noncopyable, public std::enable_shared_from_this<Connector>
{
public:
    using NewConnectionCallback = std::function<void(int sockfd)>;

    Connector(EventLoop* loop, const InetAddress& serverAddr);
    ~Connector();

    void setNewConnectionCallback(NewConnectionCallback cb) { newConnectionCallback = std::move(cb); }

    void start();
    void restart();
    void stop();

    const InetAddress& serverAddress() const { return serverAddr_; }

private:
    enum class State
    {
        kdisconnected,
        kConnecting,
        kConnected
    };

    static constexpr int kInitRetryDelayMs = 500;
    static constexpr int kMaxRetryDelayMs = 30 * 1000;

    void setState(State s) { state_.store(s, std::memory_order_relaxed); }
    void startInLoop();
    void stopInLoop();
    void connect();
    void connecting(int sockfd);
    void handleWrite();
    void handleError();
    void retry(int sockfd);
    int removeAndResetChannel();
    void resetChannel();

    EventLoop* loop_;
    InetAddress serverAddr_;
    std::atomic<bool> connect_;
    std::atomic<State> state_;
    std::unique_ptr<Channel> channel_;
    NewConnectionCallback newConnectionCallback;
    int retryDelayMs_;
};
}   // namespace mymuduo