#include "EventLoop.h"
#include "InetAddress.h"
#include "TcpServer.h"
#include <iostream>
#include <string>
#include <utility>

using namespace mymuduo;

class EchoServer
{
public:
    EchoServer(EventLoop* loop, const InetAddress& addr, std::string name)
        : loop_(loop), server_(loop, addr, std::move(name))
    {
        // 注册回调函数
        server_.setConnectionCallback([this](const TcpConnectionPtr& conn) { onConnection(conn); });
        server_.setMessageCallback(
            [this](const TcpConnectionPtr& conn, Buffer* buf, Timestamp ts) { onMessage(conn, buf, ts); });

        // 设置合适的 loop线程数量
        server_.setThreadNum(2);
    }

    void start() { server_.start(); }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected()) {
            std::cout << "connection up: " << conn->name() << ", " << conn->peerAddress().toIpPort() << "\n";
        }
        else {
            std::cout << "connection down: " << conn->name() << ", " << conn->peerAddress().toIpPort() << "\n";
            conn->getLoop()->quit();
        }
    }
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp ts)
    {
        auto msg = buf->retrieveAllAsString();
        std::cout << "received " << msg << "\n";
        std::string str = "Hello, this server, received " + msg + " from you";
        conn->send(str);
        conn->shutdown();
    }

    EventLoop* loop_;
    TcpServer server_;
};

int main(int argc, char* argv[])
{
    InetAddress addr(8000);
    EventLoop loop;
    EchoServer server(&loop, addr, "EchoServer");
    server.start();
    loop.loop();   // 监听事件

    return 0;
}