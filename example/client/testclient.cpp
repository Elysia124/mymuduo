#include "EventLoop.h"
#include "InetAddress.h"
#include "TcpClient.h"
#include <iostream>
#include <utility>

using namespace mymuduo;
class EchoClient
{
public:
    EchoClient(EventLoop* loop, InetAddress& serverAddr, std::string name)
        : loop_(loop), client_(loop, serverAddr, std::move(name))
    {
        client_.setConnectionCallback(onConnection);
        client_.setMessageCallback(onMessage);
    }

    void connect() { client_.connect(); }

private:
    static void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected()) {
            std::cout << "connection up: " << conn->name() << ", " << conn->localAddress().toIpPort() << "\n";
            conn->send("hello, this is client");
        }
        else {
            std::cout << "connection down: " << conn->name() << ", " << conn->localAddress().toIpPort() << "\n";
        }
    }

    static void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp ts)
    {
        std::string msg = buf->retrieveAllAsString();
        std::cout << "received " << msg << "\n";
        conn->send("hello server, received from you");
        conn->shutdown();
    }

    EventLoop* loop_;
    TcpClient client_;
};

int main(int argc, char* argv[])
{
    InetAddress serverAddr(8000);
    EventLoop loop;
    EchoClient client(&loop, serverAddr, "EchoClient");
    client.connect();
    loop.loop();

    return 0;
}