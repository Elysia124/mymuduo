#include "InetAddress.h"
#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>

using namespace mymuduo;

InetAddress::InetAddress(uint16_t port, const std::string& ip)
{
    addr_.sin_family = AF_INET;
    addr_.sin_port = ::htons(port);
    ::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
}

std::string InetAddress::toIp() const
{
    char ip[INET_ADDRSTRLEN]{};
    ::inet_ntop(AF_INET, &addr_.sin_addr, ip, sizeof(ip));
    return ip;
}

std::string InetAddress::toIpPort() const
{
    std::string ipPort = toIp();
    ipPort += ":" + std::to_string(::ntohs(addr_.sin_port));
    return ipPort;
}

uint16_t InetAddress::toPort() const
{
    return ::ntohs(addr_.sin_port);
}
