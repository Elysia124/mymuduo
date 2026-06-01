#pragma once

#include "net/Logger.h"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#define CHECK_TRUE(expr)                                                                          \
    do {                                                                                          \
        if (!(expr)) {                                                                            \
            std::cerr << "CHECK failed: " #expr << " at " << __FILE__ << ':' << __LINE__ << '\n'; \
            std::abort();                                                                         \
        }                                                                                         \
    } while (false)

#define CHECK_EQ(lhs, rhs) CHECK_TRUE((lhs) == (rhs))

inline void quietOutput(const char*, std::size_t) {}
inline void quietFlush() {}

inline void quietLogger()
{
    mymuduo::Logger::disableLogging();
}

inline uint16_t pickFreePort()
{
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    CHECK_TRUE(fd >= 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    CHECK_TRUE(::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);

    socklen_t len = sizeof(addr);
    CHECK_TRUE(::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0);

    uint16_t port = ::ntohs(addr.sin_port);
    ::close(fd);
    return port;
}

inline int connectToLocalhost(uint16_t port, int timeoutMs = 3000)
{
    using namespace std::chrono;
    const auto deadline = steady_clock::now() + milliseconds(timeoutMs);

    while (steady_clock::now() < deadline) {
        int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        CHECK_TRUE(fd >= 0);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = ::htons(port);
        CHECK_TRUE(::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);

        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
            return fd;
        }

        ::close(fd);
        std::this_thread::sleep_for(milliseconds(10));
    }

    std::cerr << "connectToLocalhost timeout, port=" << port << '\n';
    std::abort();
}

inline void sendAll(int fd, std::string_view data)
{
    std::size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, 0);
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
        }
        else if (n < 0 && errno == EINTR) {
            continue;
        }
        else {
            std::cerr << "send failed errno=" << errno << " error=" << std::strerror(errno) << '\n';
            std::abort();
        }
    }
}

inline std::string recvExactly(int fd, std::size_t nbytes)
{
    std::string result;
    result.resize(nbytes);

    std::size_t received = 0;
    while (received < nbytes) {
        ssize_t n = ::recv(fd, result.data() + received, nbytes - received, 0);
        if (n > 0) {
            received += static_cast<std::size_t>(n);
        }
        else if (n == 0) {
            std::cerr << "peer closed before receiving enough bytes, got=" << received
                      << " expected=" << nbytes << '\n';
            std::abort();
        }
        else if (errno == EINTR) {
            continue;
        }
        else {
            std::cerr << "recv failed errno=" << errno << " error=" << std::strerror(errno) << '\n';
            std::abort();
        }
    }

    return result;
}
