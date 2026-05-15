#include "Callbacks.h"
#include "Buffer.h"

namespace mymuduo {

void defaultConnectionCallback(const TcpConnectionPtr& conn) {}

void defaultMessageCallback(const TcpConnectionPtr& conn, Buffer* buffer, Timestamp receiveTime)
{
    buffer->retrieveAll();
}

}   // namespace mymuduo