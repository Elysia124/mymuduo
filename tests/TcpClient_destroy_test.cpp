#include "net/Buffer.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/TcpClient.h"
#include "net/TcpConnection.h"
#include "net/TcpServer.h"
#include "test_util.h"

#include <atomic>
#include <memory>

using namespace mymuduo;

namespace {
void testDestroyWhileRetrying()
{
    EventLoop loop;
    uint16_t unusedPort = pickFreePort();

    std::unique_ptr<TcpClient> client =
        std::make_unique<TcpClient>(&loop, InetAddress(unusedPort), "DestroyRetryingClient");
    client->enableRetry();
    client->connect();

    loop.runAfter(0.250, [&] {
        client.reset();
    });

    // TcpClient 析构时会延迟 1 秒释放 connector，继续跑一会儿验证没有悬空回调。
    loop.runAfter(1.500, [&] {
        loop.quit();
    });

    loop.loop();
    CHECK_TRUE(client == nullptr);
}

void testDestroyWhileConnected()
{
    EventLoop loop;
    uint16_t port = pickFreePort();

    TcpServer server(&loop, InetAddress(port), "DestroyConnectedServer");
    std::unique_ptr<TcpClient> client;

    std::atomic<int> serverUp{0};
    std::atomic<int> serverDown{0};
    std::atomic<bool> clientConnected{false};

    server.setConnectionCallback([&](const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            serverUp.fetch_add(1, std::memory_order_relaxed);
        }
        else {
            serverDown.fetch_add(1, std::memory_order_relaxed);
            loop.quit();
        }
    });

    server.start();

    client = std::make_unique<TcpClient>(&loop, InetAddress(port), "DestroyConnectedClient");
    client->setConnectionCallback([&](const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            clientConnected.store(true, std::memory_order_relaxed);
            conn->getLoop()->runAfter(0.050, [&] {
                client.reset();
            });
        }
    });
    client->connect();

    loop.runAfter(3.0, [&] {
        std::cerr << "testDestroyWhileConnected timeout serverUp=" << serverUp.load(std::memory_order_relaxed)
                  << " serverDown=" << serverDown.load(std::memory_order_relaxed)
                  << " clientConnected=" << clientConnected.load(std::memory_order_relaxed) << '\n';
        std::abort();
    });

    loop.loop();

    CHECK_TRUE(client == nullptr);
    CHECK_TRUE(clientConnected.load(std::memory_order_relaxed));
    CHECK_EQ(serverUp.load(std::memory_order_relaxed), 1);
    CHECK_EQ(serverDown.load(std::memory_order_relaxed), 1);
}

void testDestroyIdleClient()
{
    EventLoop loop;
    uint16_t unusedPort = pickFreePort();

    auto client = std::make_unique<TcpClient>(&loop, InetAddress(unusedPort), "DestroyIdleClient");
    client.reset();

    loop.runAfter(0.050, [&] {
        loop.quit();
    });
    loop.loop();
}
} // namespace

int main()
{
    quietLogger();

    testDestroyIdleClient();
    testDestroyWhileRetrying();
    testDestroyWhileConnected();

    std::cout << "TcpClient_destroy_test passed\n";
    return 0;
}
