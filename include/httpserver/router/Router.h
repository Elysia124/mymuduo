#pragma once

#include "httpserver/http/HttpRequest.h"
#include "httpserver/http/HttpResponse.h"
#include "httpserver/router/PathParams.h"
#include "httpserver/router/RouterTrie.h"
#include "net/noncopyable.h"
#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>


namespace mymuduo::http::router {
class RouterHandler;


// 包含两个路由处理器：对象式路由处理器和回调式路由处理器
// 如果是简单处理可注册回调式路由处理器
// 如果式复杂处理可注册对象式路由处理器(需继承 RouterHandler)
class Router : noncopyable
{
public:
    using HandlerPtr = std::shared_ptr<RouterHandler>;
    using HandlerCallback = std::function<void(const HttpRequest&, const PathParams&, HttpResponse*)>;

    bool route(const HttpRequest& req, HttpResponse* resp) const;

    void Get(std::string_view path, HandlerCallback cb)
    {
        addRoute(HttpRequest::Method::kGet, path, std::move(cb));
    }
    void Get(std::string_view path, HandlerPtr cb)
    {
        addRoute(HttpRequest::Method::kGet, path, std::move(cb));
    }

    void Post(std::string_view path, HandlerCallback cb)
    {
        addRoute(HttpRequest::Method::kPost, path, std::move(cb));
    }
    void Post(std::string_view path, HandlerPtr cb)
    {
        addRoute(HttpRequest::Method::kPost, path, std::move(cb));
    }

    void Head(std::string_view path, HandlerCallback cb)
    {
        addRoute(HttpRequest::Method::kHead, path, std::move(cb));
    }
    void Head(std::string_view path, HandlerPtr cb)
    {
        addRoute(HttpRequest::Method::kHead, path, std::move(cb));
    }

    void Put(std::string_view path, HandlerCallback cb)
    {
        addRoute(HttpRequest::Method::kPut, path, std::move(cb));
    }
    void Put(std::string_view path, HandlerPtr cb)
    {
        addRoute(HttpRequest::Method::kPut, path, std::move(cb));
    }

    void Delete(std::string_view path, HandlerCallback cb)
    {
        addRoute(HttpRequest::Method::kDelete, path, std::move(cb));
    }
    void Delete(std::string_view path, HandlerPtr cb)
    {
        addRoute(HttpRequest::Method::kDelete, path, std::move(cb));
    }

    void Options(std::string_view path, HandlerCallback cb)
    {
        addRoute(HttpRequest::Method::kOptions, path, std::move(cb));
    }
    void Options(std::string_view path, HandlerPtr cb)
    {
        addRoute(HttpRequest::Method::kOptions, path, std::move(cb));
    }

private:
    void addRoute(HttpRequest::Method method, std::string_view path, HandlerCallback cb);
    void addRoute(HttpRequest::Method method, std::string_view path, HandlerPtr ptr);

    struct RouterEntry
    {
        HandlerCallback handlerCallback;
        HandlerPtr handlerPtr;

        bool isHandlerCallback() const { return static_cast<bool>(handlerCallback); }

        bool isHandlerPtr() const { return static_cast<bool>(handlerPtr); }
    };

    static constexpr std::size_t kMethodNum = static_cast<size_t>(HttpRequest::Method::kOptions) + 1;
    static std::size_t methodIndex(HttpRequest::Method method) { return static_cast<std::size_t>(method); }

    std::array<RouterTrie, kMethodNum> tries_;   // 每种 method 对应一个路由字典树
    std::vector<RouterEntry> routes_;            // 路由处理器
};

}   // namespace mymuduo::http::router