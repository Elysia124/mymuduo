#include "EPollPoller.h"
#include "Poller.h"
#include <cstdlib>

using namespace mymuduo;

Poller* Poller::newDefaultPoller(EventLoop* loop)
{
    if (::getenv("MODUO_USE_POLL")) {
        return nullptr;
    }
    else {
        return new EPollPoller(loop);
    }
}