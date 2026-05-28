#pragma once

#include "httpserver/router/PathParams.h"
namespace mymuduo::http {
class HttpRequest;
class HttpResponse;
}   // namespace mymuduo::http
namespace mymuduo::http::router {
class RouterHandler
{
public:
    virtual ~RouterHandler() = default;
    virtual void handle(const HttpRequest& req, const PathParams& params,HttpResponse* resp) = 0;
};
}   // namespace mymuduo::http::router