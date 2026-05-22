#include "Buffer.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "TcpClient.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include "test_util.h"

#include <atomic>
#include <memory>

using namespace mymuduo;

int main()
{
    quietLogger();

    EventLoop loop;
    uint16_t port = pickFreePort();
    InetAddress serverAddr(port);

    std::unique_ptr<TcpServer> server;
    std::atomic<bool> connected{false};
    std::atomic<bool> gotPong{false};

    TcpClient client(&loop, serverAddr, "RetryClientTest");
    client.enableRetry();
    client.setConnectionCallback([&](const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            connected.store(true, std::memory_order_relaxed);
            conn->send("ping");
        }
    });
    client.setMessageCallback([&](const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
        std::string msg = buf->retrieveAllAsString();
        if (msg == "pong") {
            gotPong.store(true, std::memory_order_relaxed);
            conn->shutdown();
            loop.quit();
        }
    });

    // 先启动 client，让它连接失败并进入 Connector::retry。
    client.connect();

    // 过一会儿再启动 server，验证 TcpClient 能重连成功。
    loop.runAfter(0.700, [&] {
        server = std::make_unique<TcpServer>(&loop, InetAddress(port), "RetryServerTest");
        server->setMessageCallback([](const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
            std::string msg = buf->retrieveAllAsString();
            if (msg == "ping") {
                conn->send("pong");
            }
        });
        server->start();
    });

    loop.runAfter(4.0, [&] {
        std::cerr << "TcpClient_retry_test timeout connected=" << connected.load(std::memory_order_relaxed)
                  << " gotPong=" << gotPong.load(std::memory_order_relaxed) << '\n';
        std::abort();
    });

    loop.loop();

    CHECK_TRUE(connected.load(std::memory_order_relaxed));
    CHECK_TRUE(gotPong.load(std::memory_order_relaxed));

    std::cout << "TcpClient_retry_test passed\n";
    return 0;
}
