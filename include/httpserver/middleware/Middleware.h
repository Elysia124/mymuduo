#pragma once

namespace mymuduo::http {
class HttpRequest;
class HttpResponse;
}   // namespace mymuduo::http

namespace mymuduo::http::middleware {
class Middleware
{
public:
    virtual ~Middleware() = default;

    // 请求前处理
    // 返回 true: 继续进入路由
    // 返回 false: 中断路由，直接使用 response 返回
    virtual bool before(const HttpRequest&, HttpResponse&) { return true; };

    // 默认实现为转发至 before(const HttpRequest&, HttpResponse)
    virtual bool before(HttpRequest& req, HttpResponse& resp)
    {
        return before(static_cast<const HttpRequest&>(req), resp);
    };

    // 响应发送前调用，可统一修改响应头、Cookie、日志等。
    virtual void after(const HttpRequest&, HttpResponse&) {};
};
}   // namespace mymuduo::http::middleware