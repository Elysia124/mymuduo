#pragma once

#include "net/noncopyable.h"
#include <memory>
#include <vector>

namespace mymuduo::http {
class HttpRequest;
class HttpResponse;
}   // namespace mymuduo::http

namespace mymuduo::http::middleware {
class Middleware;
class MiddlewareChain : noncopyable
{
public:
    void addMiddleware(std::shared_ptr<Middleware> middleware);

    bool processBefore(HttpRequest& req, HttpResponse& resp);

    void processAfter(const HttpRequest& req, HttpResponse& resp);

private:
    std::vector<std::shared_ptr<Middleware>> middlewares_;
};
}   // namespace mymuduo::http::middleware