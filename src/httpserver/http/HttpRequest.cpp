#include "httpserver/http/HttpRequest.h"
#include "httpserver/utils/HttpFuncs.h"
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>



using namespace mymuduo::http;

bool HttpRequest::setMethod(const char* start, const char* end)
{
    assert(method_ == Method::kInvalid);
    auto str = std::string_view(start, end);
    if (str == "GET") {
        method_ = Method::kGet;
    }
    else if (str == "POST") {
        method_ = Method::kPost;
    }
    else if (str == "PUT") {
        method_ = Method::kPut;
    }
    else if (str == "HEAD") {
        method_ = Method::kHead;
    }
    else if (str == "DELETE") {
        method_ = Method::kDelete;
    }
    else if (str == "OPTIONS") {
        method_ = Method::kOptions;
    }
    else {
        method_ = Method::kInvalid;
    }
    return method_ != Method::kInvalid;
}

bool HttpRequest::setVersion(std::string_view version)
{
    if (version == "HTTP/1.1") {
        version_ = Version::kHttp11;
    }
    else if (version == "HTTP/1.0") {
        version_ = Version::kHttp10;
    }
    else {
        version_ = Version::kUnknown;
    }

    return version_ != Version::kUnknown;
}

// 从 '?' 后分离
void HttpRequest::setQueryParam(const char* start, const char* end)
{
    std::string_view str(start, end);
    std::size_t prev = 0;
    while (prev < str.size()) {
        std::size_t ampPos = str.find('&', prev);   //'&' 的位置
        if (ampPos == std::string_view::npos) {
            ampPos = str.size();
        }

        auto pair = str.substr(prev, ampPos - prev);   // 得到一组 "key=value"

        if (!pair.empty()) {
            std::size_t equalPos = pair.find('=');

            if (equalPos == std::string_view::npos) {   // value 为空
                queryParams_[urlDecode(pair)] = "";
            }
            else {
                std::string_view key = pair.substr(0, equalPos);
                std::string_view value = pair.substr(equalPos + 1);

                if (!key.empty()) {
                    queryParams_[urlDecode(key)] = urlDecode(value);
                }
            }
        }

        if (ampPos == str.size()) {
            break;
        }

        prev = ampPos + 1;
    }
}

void HttpRequest::addHeader(const char* start, const char* colon, const char* end)
{
    std::string key(start, colon);
    ++colon;   // 去掉冒号
    std::string value(colon, end);

    trim(key);
    trim(value);

    // 把 key 转化为小写
    stringToLower(key);

    // 如果新添加的是 cookie，则让 cookie 缓存失效
    if (key == "cookie") {
        cookies_.clear();
        cookiesParsed_ = false;
    }

    headers_[std::move(key)].push_back(std::move(value));
}


std::optional<std::string> HttpRequest::getQueryParam(std::string_view key) const
{
    auto it = queryParams_.find(key);
    if (it == queryParams_.end()) {
        return std::nullopt;
    }

    return it->second;
}

// 返回 key 包含的 value 的第一个内容
std::optional<std::string> HttpRequest::getHeader(std::string_view field) const
{
    auto it = headers_.find(field);
    if (it == headers_.end()) {
        return std::nullopt;
    }

    return it->second.front();
}

// 返回 key 包含的全部 value
std::vector<std::string> HttpRequest::getHeaders(std::string_view field) const
{
    auto it = headers_.find(field);
    if (it == headers_.end()) {
        return {};
    }

    return it->second;
}

std::optional<std::string> HttpRequest::getCookie(std::string_view name) const
{
    if (!cookiesParsed_) {
        parseCookies();
    }

    auto it = cookies_.find(name);
    if (it == cookies_.end()) {
        return std::nullopt;
    }

    return it->second;
}

// 解析 Cookie
void HttpRequest::parseCookies() const
{
    cookiesParsed_ = true;
    cookies_.clear();

    auto cookieHeaders = getHeaders("Cookie");
    if (cookieHeaders.empty()) {
        return;
    }

    for (const auto& cookieStr : cookieHeaders)

    {
        std::string_view cookie(cookieStr);
        std::size_t start = 0;

        while (start < cookie.size()) {
            std::size_t semicolonPos = cookie.find(';', start);
            if (semicolonPos == std::string_view::npos) {
                semicolonPos = cookie.size();
            }

            std::string_view pair = cookie.substr(start, semicolonPos - start);
            std::size_t equalPos = pair.find('=');

            if (equalPos != std::string_view::npos) {
                std::string key(pair.substr(0, equalPos));
                std::string value(pair.substr(equalPos + 1));

                trim(key);
                trim(value);

                if (!key.empty()) {
                    cookies_[std::move(key)] = std::move(value);
                }
            }

            if (semicolonPos == cookie.size()) {
                break;
            }

            start = semicolonPos + 1;
        }
    }
}


void HttpRequest::swap(HttpRequest& other)
{
    std::swap(method_, other.method_);
    std::swap(version_, other.version_);
    std::swap(path_, other.path_);
    std::swap(queryParams_, other.queryParams_);
    std::swap(receiveTime_, other.receiveTime_);
    std::swap(headers_, other.headers_);
    std::swap(body_, other.body_);
    std::swap(bodyLength_, other.bodyLength_);
    std::swap(cookiesParsed_, other.cookiesParsed_);
    std::swap(cookies_, other.cookies_);
}