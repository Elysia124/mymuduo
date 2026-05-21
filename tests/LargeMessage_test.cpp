#include "Buffer.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include "test_util.h"

#include <thread>

using namespace mymuduo;

int main()
{
    quietLogger();

    EventLoop loop;
    uint16_t port = pickFreePort();

    TcpServer server(&loop, InetAddress(port), "LargeMessageTest");
    server.setThreadNum(2);
    server.setMessageCallback([](const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
        std::string msg = buf->retrieveAllAsString();
        conn->send(std::move(msg));
    });
    server.start();

    std::string big;
    big.resize(1024 * 1024);
    for (size_t i = 0; i < big.size(); ++i) {
        big[i] = static_cast<char>('a' + (i % 26));
    }

    std::thread client([&] {
        int fd = connectToLocalhost(port);
        sendAll(fd, big);
        std::string echoed = recvExactly(fd, big.size());
        CHECK_EQ(echoed, big);
        ::close(fd);
        loop.queueInLoop([&] { loop.quit(); });
    });

    loop.runAfter(5.0, [&] {
        std::cerr << "LargeMessage_test timeout\n";
        std::abort();
    });
    loop.loop();
    client.join();

    std::cout << "LargeMessage_test passed\n";
    return 0;
}
