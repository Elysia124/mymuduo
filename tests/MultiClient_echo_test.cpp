#include "Buffer.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include "test_util.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace mymuduo;

int main()
{
    quietLogger();

    EventLoop loop;
    uint16_t port = pickFreePort();

    TcpServer server(&loop, InetAddress(port), "MultiClientEchoTest");
    server.setThreadNum(4);
    server.setMessageCallback([](const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
    });
    server.start();

    constexpr int kClients = 100;
    std::atomic<int> ok{0};

    std::vector<std::thread> clients;
    clients.reserve(kClients);
    for (int i = 0; i < kClients; ++i) {
        clients.emplace_back([&, i] {
            std::string msg = "client-" + std::to_string(i) + " hello";
            int fd = connectToLocalhost(port);
            sendAll(fd, msg);
            CHECK_EQ(recvExactly(fd, msg.size()), msg);
            ::close(fd);

            if (ok.fetch_add(1, std::memory_order_relaxed) + 1 == kClients) {
                loop.queueInLoop([&] { loop.quit(); });
            }
        });
    }

    loop.runAfter(5.0, [&] {
        std::cerr << "MultiClient_echo_test timeout, ok=" << ok.load(std::memory_order_relaxed) << '\n';
        std::abort();
    });
    loop.loop();

    for (auto& t : clients) {
        t.join();
    }

    CHECK_EQ(ok.load(std::memory_order_relaxed), kClients);

    std::cout << "MultiClient_echo_test passed\n";
    return 0;
}
