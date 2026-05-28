#include "net/Buffer.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/TcpConnection.h"
#include "net/TcpServer.h"
#include "test_util.h"

#include <atomic>
#include <thread>

using namespace mymuduo;

int main()
{
    quietLogger();

    EventLoop loop;
    uint16_t port = pickFreePort();

    TcpServer server(&loop, InetAddress(port), "EchoServerTest");
    std::atomic<int> upCount{0};
    std::atomic<int> downCount{0};

    server.setThreadNum(0);
    server.setConnectionCallback([&](const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            upCount.fetch_add(1, std::memory_order_relaxed);
        }
        else {
            downCount.fetch_add(1, std::memory_order_relaxed);
        }
    });
    server.setMessageCallback([](const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
    });
    server.start();

    const std::string msg = "hello mymuduo echo";
    std::thread client([&] {
        int fd = connectToLocalhost(port);
        sendAll(fd, msg);
        CHECK_EQ(recvExactly(fd, msg.size()), msg);
        ::close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        loop.queueInLoop([&] { loop.quit(); });
    });

    loop.runAfter(3.0, [&] {
        std::cerr << "TcpServer_echo_test timeout\n";
        std::abort();
    });
    loop.loop();
    client.join();

    CHECK_EQ(upCount.load(std::memory_order_relaxed), 1);

    std::cout << "TcpServer_echo_test passed\n";
    return 0;
}
