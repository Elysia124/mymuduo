#include "httpserver/middleware/cors/CorsMiddleware.h"
#include "httpserver/http/HttpRequest.h"
#include "httpserver/http/HttpResponse.h"
#include "httpserver/middleware/cors/CorsConfig.h"
#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <utility>

using namespace mymuduo::http::middleware;

CorsMiddleware::CorsMiddleware(CorsConfig config) : corsConfig_(std::move(config)){};

bool CorsMiddleware::before(const HttpRequest& req, HttpResponse& resp)
{
    // 检查是不是跨域请求
    auto origin = req.getHeader("Origin");
    if (!origin.has_value()) {
        // 没有 Origin 头，非跨域请求，直接返回不处理
        return true;
    }

    if (!isOriginAllowed(origin.value())) {   // 这个 Origin 是否在白名单内
        resp.setStatusCode(HttpResponse::HttpStatusCode::k403Forbidden);
        resp.setStatusMessage("Forbidden");
        resp.setContentType("text/plain");
        resp.setBody("CORS origin forbidden\n");
        return false;
    }

    // 检查是否是 CORS 预检请求
    const bool isPreflight = req.method() == HttpRequest::Method::kOptions &&
                             req.getHeader("Access-Control-Request-Method").has_value();
    if (!isPreflight) {
        return true;   // 如果是普通的跨域请求，放行
    }

    // 处理预检请求
    addCoresHeaders(req, resp);
    resp.setStatusCode(HttpResponse::HttpStatusCode::k204NoContent);
    resp.setStatusMessage("No Content");
    resp.setBody("");
    return false;   // 预检请求不进入业务逻辑
}

void CorsMiddleware::after(const HttpRequest& req, HttpResponse& resp)
{
    auto origin = req.getHeader("Origin");
    if (!origin.has_value() || !isOriginAllowed(origin.value())) {
        // 如果不是跨域请求或者不合法的跨域请求，直接返回
        return;
    }

    // 为 response 加上跨域请求头
    addCoresHeaders(req, resp);
}

bool CorsMiddleware::isOriginAllowed(std::string_view origin) const
{
    return std::find(corsConfig_.allowedOrigins.begin(), corsConfig_.allowedOrigins.end(), "*") !=
               corsConfig_.allowedOrigins.end() ||
           std::find(corsConfig_.allowedOrigins.begin(), corsConfig_.allowedOrigins.end(), origin) !=
               corsConfig_.allowedOrigins.end();
}

std::string CorsMiddleware::resolveAllowOrigin(std::string_view origin) const
{
    const bool allowAll = std::find(corsConfig_.allowedOrigins.begin(), corsConfig_.allowedOrigins.end(), "*") !=
                          corsConfig_.allowedOrigins.end();

    // 带 Cookie/Authorization 等凭据时，浏览器不接受 Access-Control-Allow-Origin: *
    if (allowAll && !corsConfig_.allowCredentials) {
        return "*";
    }

    return std::string(origin);
}

void CorsMiddleware::addCoresHeaders(const HttpRequest& req, HttpResponse& resp) const
{
    auto origin = req.getHeader("Origin");
    if (!origin.has_value()) {
        return;
    }

    // 设置允许的来源
    resp.setHeader("Access-Control-Allow-Origin", resolveAllowOrigin(origin.value()));
    // 告知缓存服务器或浏览器，根据不同的 Origin 缓存不同的结果，防止响应混乱
    resp.setHeader("Vary", "Origin");

    // 是否允许携带 Cookie
    if (corsConfig_.allowCredentials) {
        resp.setHeader("Access-Control-Allow-Credentials", "true");
    }

    // 设置允许的方法
    if (!corsConfig_.allowedMethods.empty()) {
        resp.setHeader("Access-Control-Allow-Methods", join(corsConfig_.allowedMethods, ", "));
    }

    // 设置允许的自定义请求头
    if (!corsConfig_.allowedHeaders.empty()) {
        resp.setHeader("Access-Control-Allow-Headers", join(corsConfig_.allowedHeaders, ", "));
    }

    // 设置预检请求的缓存有效期
    resp.setHeader("Access-Control-Max-Age", std::to_string(corsConfig_.maxAge));
}

std::string CorsMiddleware::join(const std::vector<std::string>& values, std::string_view delimiter)
{
    std::ostringstream ss;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            ss << delimiter;
        }

        ss << values[i];
    }

    return ss.str();
}
