#include <cstdint>
#include <netinet/in.h>
#include <string>

namespace mymuduo {
// 封装 socket 地址类型
class InetAddress
{
public:
    explicit InetAddress(uint16_t port, const std::string& ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in& addr) : addr_(addr){};

    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in* getSockAddr() const { return &addr_; };

private:
    sockaddr_in addr_{};
};
}   // namespace mymuduo