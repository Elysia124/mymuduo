#pragma once

#include "httpserver/middleware/Middleware.h"
#include "httpserver/middleware/cors/CorsConfig.h"
#include <string>
#include <string_view>
#include <vector>

namespace mymuduo::http {
class HttpRequest;
class HttpResponse;
}   // namespace mymuduo::http

namespace mymuduo::http::middleware {

class CorsMiddleware : public Middleware
{
public:
    explicit CorsMiddleware(CorsConfig config = CorsConfig::defaultConfig());

    bool before(const HttpRequest& req, HttpResponse& resp) override;
    void after(const HttpRequest& req, HttpResponse& resp) override;

private:
    bool isOriginAllowed(std::string_view origin) const;
    std::string resolveAllowOrigin(std::string_view origin) const;
    void addCoresHeaders(const HttpRequest& req, HttpResponse& resp) const;
    static std::string join(const std::vector<std::string>& values, std::string_view delimiter);

    CorsConfig corsConfig_;
};
}   // namespace mymuduo::http::middleware