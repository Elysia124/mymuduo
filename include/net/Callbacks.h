#pragma once

#include "net/Timestamp.h"
#include <cstddef>
#include <functional>
#include <memory>

namespace mymuduo {
class Buffer;
class TcpConnection;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using TimerCallback = std::function<void()>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*, Timestamp)>;
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, std::size_t)>;

void defaultConnectionCallback(const TcpConnectionPtr& conn);
void defaultMessageCallback(const TcpConnectionPtr& conn, Buffer* buffer, Timestamp receiveTime);

template<typename T>
inline T* get_pointer(const std::shared_ptr<T>& ptr)
{
    return ptr.get();
}

template<typename T>
inline T* get_pointer(const std::unique_ptr<T>& ptr)
{
    return ptr.get();
}

}   // namespace mymuduo
