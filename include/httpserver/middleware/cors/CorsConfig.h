#pragma once

#include <string>
#include <vector>
namespace mymuduo::http::middleware {
struct CorsConfig
{
    std::vector<std::string> allowedOrigins;
    std::vector<std::string> allowedMethods;
    std::vector<std::string> allowedHeaders;

    bool allowCredentials = false;
    int maxAge = 3600;

    static CorsConfig defaultConfig()
    {
        CorsConfig config;
        config.allowedOrigins = {"*"};
        config.allowedMethods = {"GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS"};
        config.allowedHeaders = {"Content-Type", "Authorization", "Cookie"};

        return config;
    }
};
}   // namespace mymuduo::http::middleware