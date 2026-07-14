#include "httpserver/middleware/MiddlewareChain.h"
#include "httpserver/http/HttpRequest.h"
#include "httpserver/http/HttpResponse.h"
#include "httpserver/middleware/Middleware.h"
#include "net/Logger.h"
#include <exception>
#include <utility>

using namespace mymuduo::http::middleware;

void MiddlewareChain::addMiddleware(std::shared_ptr<Middleware> middleware)
{
    if (middleware) {
        middlewares_.push_back(std::move(middleware));
    }
}

bool MiddlewareChain::processBefore(HttpRequest& req, HttpResponse& resp)
{
    for (const auto& middleware : middlewares_) {
        if (middleware && !middleware->before(req, resp)) {
            return false;
        }
    }
    return true;
}

void MiddlewareChain::processAfter(const HttpRequest& req, HttpResponse& resp)
{
    // 反方向处理响应
    for (auto it = middlewares_.rbegin(); it != middlewares_.rend(); ++it) {
        const auto& middleware = *it;
        try {
            if (middleware) {
                middleware->after(req, resp);
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR << "middleware after error: " << e.what();
        }
    }
}
