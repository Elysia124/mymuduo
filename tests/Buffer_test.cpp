#include "Buffer.h"
#include "test_util.h"

#include <string>
#include <sys/socket.h>
#include <unistd.h>

using namespace mymuduo;

int main()
{
    quietLogger();

    Buffer buf;
    CHECK_EQ(buf.readableBytes(), 0u);
    CHECK_EQ(buf.writableBytes(), Buffer::kInitialSize);
    CHECK_EQ(buf.prependableBytes(), Buffer::kCheapPrepend);

    const std::string hello = "hello";
    buf.append(hello.data(), hello.size());
    CHECK_EQ(buf.readableBytes(), 5u);
    CHECK_EQ(buf.retrieveAsString(2), std::string("he"));
    CHECK_EQ(buf.readableBytes(), 3u);
    CHECK_EQ(buf.retrieveAllAsString(), std::string("llo"));
    CHECK_EQ(buf.readableBytes(), 0u);
    CHECK_EQ(buf.prependableBytes(), Buffer::kCheapPrepend);

    std::string big(1024 * 1024, 'x');
    buf.append(big.data(), big.size());
    CHECK_EQ(buf.readableBytes(), big.size());
    CHECK_EQ(buf.retrieveAllAsString(), big);

    // 测试 retrieve 后移动已有数据，而不是无脑扩容。
    std::string a(900, 'a');
    std::string b(900, 'b');
    buf.append(a.data(), a.size());
    buf.retrieve(800);
    const size_t oldPrependable = buf.prependableBytes();
    buf.append(b.data(), b.size());
    CHECK_TRUE(buf.prependableBytes() <= oldPrependable); // 触发 makeSpace 后会把可读数据搬回 kCheapPrepend
    CHECK_EQ(buf.readableBytes(), 100u + b.size());
    CHECK_EQ(buf.retrieveAsString(100), std::string(100, 'a'));
    CHECK_EQ(buf.retrieveAllAsString(), b);

    // readFd / writeFd 基础测试。
    int sv[2];
    CHECK_TRUE(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    std::string msg = "readFd/writeFd message";
    sendAll(sv[0], msg);

    int savedErrno = 0;
    ssize_t n = buf.readFd(sv[1], savedErrno);
    CHECK_EQ(n, static_cast<ssize_t>(msg.size()));
    CHECK_EQ(buf.retrieveAllAsString(), msg);

    buf.append(msg.data(), msg.size());
    n = buf.writeFd(sv[1], savedErrno);
    CHECK_EQ(n, static_cast<ssize_t>(msg.size()));
    CHECK_EQ(recvExactly(sv[0], msg.size()), msg);

    ::close(sv[0]);
    ::close(sv[1]);

    std::cout << "Buffer_test passed\n";
    return 0;
}
