#include "httpserver/http/HttpResponse.h"
#include "httpserver/utils/StringHash.h"
#include "net/Buffer.h"
#include <cstdio>
#include <string_view>
#include <utility>

using namespace mymuduo::http;

bool HttpResponse::isManagedHeader(std::string_view key)
{
    CaseInsensitiveStringEqual equal;
    return equal(key, "Connection") || equal(key, "Content-Length");
}

void HttpResponse::applyManagedHeader(std::string_view key, std::string_view value)
{
    CaseInsensitiveStringEqual equal;
    if (equal(key, "Connection")) {
        if (equal(value, "close")) {
            closeConnection_ = true;
        }
        else if (equal(value, "Keep-Alive")) {
            closeConnection_ = false;
        }
    }
}

void HttpResponse::appendHeader(std::string key, std::string value)
{
    if (isManagedHeader(key)) {
        applyManagedHeader(key, value);
        return;
    }

    headers_.emplace_back(std::move(key), std::move(value));
}

void HttpResponse::appendToBuffer(Buffer* buffer, bool sendBody) const
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%s %d ", httpVersion_.c_str(), static_cast<int>(statusCode_));

    buffer->append(buf);
    buffer->append(statusMessage_);
    buffer->append("\r\n");

    if (closeConnection_) {
        buffer->append("Connection: close\r\n");
    }
    else {
        buffer->append("Connection: Keep-Alive\r\n");
    }

    snprintf(buf, sizeof(buf), "Content-Length: %zu\r\n", body_.size());
    buffer->append(buf);

    for (const auto& header : headers_) {
        buffer->append(header.first);
        buffer->append(": ");
        buffer->append(header.second);
        buffer->append("\r\n");
    }

    buffer->append("\r\n");
    if (sendBody) {
        buffer->append(body_);
    }
}

void HttpResponse::setHeader(std::string key, std::string value)
{
    if (isManagedHeader(key)) {
        applyManagedHeader(key, value);
        return;
    }

    bool replaced = false;

    CaseInsensitiveStringEqual equal;
    for (auto it = headers_.begin(); it != headers_.end();) {
        if (equal(key, it->first)) {
            if (!replaced) {   // 当前重复的key还未替换过
                it->first = key;
                it->second = std::move(value);
                replaced = true;
                ++it;
            }
            else {   // 又遇到了重复的key且当前重复的key已经替换过
                it = headers_.erase(it);
            }
        }
        else {
            ++it;
        }
    }

    if (!replaced) {   // 如果是新的 key
        headers_.emplace_back(std::move(key), std::move(value));
    }
}

void HttpResponse::setCookie(std::string name, std::string value, int maxAge, bool httpOnly,
                             bool secureCookie)
{
    std::string cookie = std::move(name) + '=' + std::move(value) +
                         "; Path=/; Max-Age=" + std::to_string(maxAge) + "; SameSite=Lax";
    if (httpOnly) {
        cookie += "; HttpOnly";
    }
    if (secureCookie) {
        cookie += "; Secure";
    }

    appendHeader("Set-Cookie", std::move(cookie));
}

void HttpResponse::clearCookie(std::string name, bool secureCookie)
{
    std::string cookie = std::move(name) + "=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax";
    if (secureCookie) {
        cookie += "; Secure";
    }
    appendHeader("Set-Cookie", std::move(cookie));
}
