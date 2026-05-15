#include "Acceptor.h"
#include "InetAddress.h"
#include "Logger.h"
#include "Socket.h"
#include "Timestamp.h"
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

using namespace mymuduo;

namespace {
int createNonblockingOrDie()
{
    int sockfd = ::socket(AF_INET, SOCK_NONBLOCK | SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (sockfd < 0) {
        LOG_FATAL("%s:%s:%d listen socket create error: %s\n", __FILE__, __FUNCTION__, __LINE__, strerror(errno));
    }
    return sockfd;
}
}   // namespace

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop), acceptSocket_(createNonblockingOrDie()), acceptChannel_(loop, acceptSocket_.fd()), listening_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback([this](Timestamp) { handleRead(); });
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

// 新用户连接
void Acceptor::handleRead()
{
    while (true) {
        InetAddress peerAddr;
        int connfd = acceptSocket_.accept(&peerAddr);

        if (connfd >= 0) {
            if (newConnectionCallback_) {
                newConnectionCallback_(connfd, peerAddr);
            }
            else {
                ::close(connfd);
            }
        }
        else {
            if (errno == EAGAIN) {   // no more new connection
                break;
            }
            if (errno == EMFILE) {
                LOG_ERROR("%s:%s:%d sockfd reach limit\n", __FILE__, __FUNCTION__, __LINE__);
            }
            else {
                LOG_ERROR("%s:%s:%d accept error: %s\n", __FILE__, __FUNCTION__, __LINE__, strerror(errno));
            }
            break;
        }
    }
}