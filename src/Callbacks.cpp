#include "Callbacks.h"
#include "Buffer.h"

namespace mymuduo {

void defaultConnectionCallback(const TcpConnectionPtr&) {}

void defaultMessageCallback(const TcpConnectionPtr&, Buffer* buffer, Timestamp)
{
    buffer->retrieveAll();
}

}   // namespace mymuduo