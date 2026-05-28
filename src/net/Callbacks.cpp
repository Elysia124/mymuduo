#include "net/Callbacks.h"

#include "net/Buffer.h"

namespace mymuduo {

void defaultConnectionCallback(const TcpConnectionPtr& ) {}

void defaultMessageCallback(const TcpConnectionPtr&, Buffer* buffer, Timestamp)
{
    buffer->retrieveAll();
}

}   // namespace mymuduo