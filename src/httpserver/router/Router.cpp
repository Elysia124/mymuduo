#include "httpserver/router/Router.h"
#include "httpserver/http/HttpRequest.h"
#include "httpserver/router/PathParams.h"
#include "httpserver/router/RouterHandler.h"
#include "httpserver/router/RouterTrie.h"
#include <cassert>
#include <cstddef>
#include <string_view>
#include <utility>

using namespace mymuduo::http::router;

void Router::addRoute(HttpRequest::Method method, std::string_view path, HandlerCallback cb)
{
    assert(cb);
    assert(!path.empty());
    assert(path[0] == '/');

    const std::size_t index = methodIndex(method);
    assert(index < tries_.size());

    RouterEntry entry;
    entry.handlerCallback = std::move(cb);

    const std::size_t routeId = routes_.size();

    tries_[index].insert(path, routeId);
    routes_.push_back(std::move(entry));
}

void Router::addRoute(HttpRequest::Method method, std::string_view path, HandlerPtr ptr)
{
    assert(ptr);
    assert(!path.empty());
    assert(path[0] == '/');

    const std::size_t index = methodIndex(method);
    assert(index < tries_.size());

    RouterEntry entry;
    entry.handlerPtr = std::move(ptr);

    const std::size_t routeId = routes_.size();

    tries_[index].insert(path, routeId);
    routes_.push_back(std::move(entry));
}

bool Router::routeWithMethod(HttpRequest::Method method, const HttpRequest& req, HttpResponse* resp) const
{
    const std::size_t index = methodIndex(method);
    if (index >= tries_.size()) {
        return false;
    }

    std::size_t routeId = 0;
    RouterTrie::ParamList rawParams;

    if (!tries_[index].match(req.path(), routeId, rawParams)) {   // 在对应方法的字典树中匹配路径
        return false;
    }

    if (routeId >= routes_.size()) {
        return false;
    }

    const RouterEntry& entry = routes_[routeId];   // 根据 routeId 拿到对应的处理器
    PathParams params(std::move(rawParams));

    if (entry.isHandlerCallback()) {
        entry.handlerCallback(req, params, resp);
        return true;
    }

    if (entry.isHandlerPtr()) {
        entry.handlerPtr->handle(req, params, resp);
        return true;
    }

    return false;
}

bool Router::route(const HttpRequest& req, HttpResponse* resp) const
{
    const auto method = req.method();

    // HEAD 优先匹配 HEAD 路由
    if (method == HttpRequest::Method::kHead) {
        if (routeWithMethod(method, req, resp)) {
            return true;
        }

        // 如果没有 HEAD 路由，则 fallback 到 GET 路由
        return routeWithMethod(HttpRequest::Method::kGet, req, resp);
    }

    return routeWithMethod(method, req, resp);
}