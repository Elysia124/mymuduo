#include "Buffer.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "TcpServer.h"
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
            ++upCount;
        }
        else {
            ++downCount;
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
                    ++successCount;
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
        std::cerr << "ManyClients_roundtrip_test timeout up=" << upCount.load()
                  << " down=" << downCount.load()
                  << " success=" << successCount.load() << '\n';
        std::abort();
    });

    loop.loop();
    clients.join();

    CHECK_EQ(successCount.load(), kClients);
    CHECK_EQ(upCount.load(), kClients);

    std::cout << "ManyClients_roundtrip_test passed\n";
    return 0;
}
