#include "net/Buffer.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/TcpConnection.h"
#include "net/TcpServer.h"
#include "test_util.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

using namespace mymuduo;

int main()
{
    quietLogger();

    EventLoop loop;
    uint16_t port = pickFreePort();

    constexpr int kClients = 100;
    constexpr int kRounds = 10;

    TcpServer server(&loop, InetAddress(port), "ManyClientsRoundtripServer");
    server.setThreadNum(4);

    std::atomic<int> upCount{0};
    std::atomic<int> downCount{0};
    std::atomic<int> successCount{0};

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

    std::thread clients([&] {
        std::vector<std::thread> threads;
        threads.reserve(kClients);

        for (int i = 0; i < kClients; ++i) {
            threads.emplace_back([&, i] {
                int fd = connectToLocalhost(port, 5000);

                bool ok = true;
                for (int r = 0; r < kRounds; ++r) {
                    std::string msg = "client=" + std::to_string(i)
                                    + " round=" + std::to_string(r)
                                    + " payload=" + std::string(128 + (i % 17), static_cast<char>('a' + (r % 26)));
                    sendAll(fd, msg);
                    if (recvExactly(fd, msg.size()) != msg) {
                        ok = false;
                        break;
                    }
                }

                ::close(fd);
                if (ok) {
                    successCount.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        loop.quit();
    });

    loop.runAfter(10.0, [&] {
        std::cerr << "ManyClients_roundtrip_test timeout up=" << upCount.load(std::memory_order_relaxed)
                  << " down=" << downCount.load(std::memory_order_relaxed)
                  << " success=" << successCount.load(std::memory_order_relaxed) << '\n';
        std::abort();
    });

    loop.loop();
    clients.join();

    CHECK_EQ(successCount.load(std::memory_order_relaxed), kClients);
    CHECK_EQ(upCount.load(std::memory_order_relaxed), kClients);

    std::cout << "ManyClients_roundtrip_test passed\n";
    return 0;
}
