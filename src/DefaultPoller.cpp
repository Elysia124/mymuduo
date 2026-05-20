#include "EPollPoller.h"
#include "Poller.h"
#include <cstdlib>

using namespace mymuduo;

std::unique_ptr<Poller> Poller::newDefaultPoller(EventLoop* loop)
{
    // if (::getenv("MODUO_USE_POLL")) {   // not use
    //     return nullptr;
    // }
    // else {
    //      return std::make_unique<EPollPoller>(loop);
    // }
    return std::make_unique<EPollPoller>(loop);
}