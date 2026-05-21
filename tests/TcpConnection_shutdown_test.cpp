#include "Buffer.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include "test_util.h"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>

using namespace mymuduo;

namespace {
std::string recvUntilEof(int fd)
{
    std::string result;
    char buf[64 * 1024];

    for (;;) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            result.append(buf, static_cast<size_t>(n));
        }
        else if (n == 0) {
            return result;
        }
        else if (errno == EINTR) {
            continue;
        }
        else {
            std::cerr << "recvUntilEof failed errno=" << errno
                      << " error=" << std::strerror(errno) << '\n';
            std::abort();
        }
    }
}
} // namespace

int main()
{
    quietLogger();

    EventLoop loop;
    uint16_t port = pickFreePort();

    // 16 MiB 足够让本地 socket 大概率走 outputBuffer_ + EPOLLOUT 路径。
    const std::string payload(16 * 1024 * 1024, 's');

    TcpServer server(&loop, InetAddress(port), "ShutdownServerTest");
    std::atomic<int> upCount{0};
    std::atomic<int> downCount{0};
    std::atomic<int> writeCompleteCount{0};
    std::atomic<bool> clientGotAll{false};

    server.setConnectionCallback([&](const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            ++upCount;
        }
        else {
            ++downCount;
            if (clientGotAll.load()) {
                loop.quit();
            }
        }
    });

    server.setWriteCompleteCallback([&](const TcpConnectionPtr&) {
        ++writeCompleteCount;
    });

    server.setMessageCallback([&](const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
        std::string msg = buf->retrieveAllAsString();
        if (msg == "go") {
            conn->send(payload);
            // 正确语义：如果 outputBuffer_ 还有数据，应等数据写完再 shutdownWrite。
            conn->shutdown();
        }
    });

    server.start();

    std::thread client([&] {
        int fd = connectToLocalhost(port);
        sendAll(fd, "go");

        // 先不读，让服务端更容易产生 outputBuffer_ 堆积。
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::string received = recvUntilEof(fd);
        ::close(fd);

        if (received == payload) {
            clientGotAll = true;
        }
        else {
            std::cerr << "payload mismatch received=" << received.size()
                      << " expected=" << payload.size() << '\n';
        }

        loop.queueInLoop([&] {
            if (downCount.load() > 0) {
                loop.quit();
            }
        });
    });

    loop.runAfter(8.0, [&] {
        std::cerr << "TcpConnection_shutdown_test timeout up=" << upCount.load()
                  << " down=" << downCount.load()
                  << " writeComplete=" << writeCompleteCount.load()
                  << " clientGotAll=" << clientGotAll.load() << '\n';
        std::abort();
    });

    loop.loop();
    client.join();

    CHECK_EQ(upCount.load(), 1);
    CHECK_EQ(downCount.load(), 1);
    CHECK_TRUE(clientGotAll.load());
    CHECK_TRUE(writeCompleteCount.load() >= 1);

    std::cout << "TcpConnection_shutdown_test passed\n";
    return 0;
}
