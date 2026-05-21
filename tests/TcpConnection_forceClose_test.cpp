#include "EventLoop.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "TcpServer.h"
#include "test_util.h"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <thread>
#include <unistd.h>

using namespace mymuduo;

namespace {
void waitForPeerClose(int fd)
{
    char buf[128];
    for (;;) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n == 0) {
            return; // FIN
        }
        if (n > 0) {
            continue;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == ECONNRESET) {
            return; // RST 也说明连接被关了
        }
        std::cerr << "recv failed while waiting close errno=" << errno
                  << " error=" << std::strerror(errno) << '\n';
        std::abort();
    }
}
} // namespace

int main()
{
    quietLogger();

    EventLoop loop;
    uint16_t port = pickFreePort();

    TcpServer server(&loop, InetAddress(port), "ForceCloseServerTest");
    std::atomic<int> upCount{0};
    std::atomic<int> downCount{0};
    std::atomic<bool> clientSawClose{false};

    server.setConnectionCallback([&](const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            ++upCount;
            // 确保 connectEstablished 完成之后再 forceClose。
            conn->getLoop()->runAfter(0.050, [conn] {
                conn->forceClose();
            });
        }
        else {
            ++downCount;
            if (clientSawClose.load()) {
                loop.quit();
            }
        }
    });

    server.start();

    std::thread client([&] {
        int fd = connectToLocalhost(port);
        waitForPeerClose(fd);
        ::close(fd);
        clientSawClose = true;
        loop.queueInLoop([&] {
            if (downCount.load() > 0) {
                loop.quit();
            }
        });
    });

    loop.runAfter(3.0, [&] {
        std::cerr << "TcpConnection_forceClose_test timeout up=" << upCount.load()
                  << " down=" << downCount.load()
                  << " clientSawClose=" << clientSawClose.load() << '\n';
        std::abort();
    });

    loop.loop();
    client.join();

    CHECK_EQ(upCount.load(), 1);
    CHECK_EQ(downCount.load(), 1);
    CHECK_TRUE(clientSawClose.load());

    std::cout << "TcpConnection_forceClose_test passed\n";
    return 0;
}
