#pragma once

#include "httpserver/utils/StringHash.h"
#include "net/Timestamp.h"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
namespace mymuduo::http {

// 对 http 请求报文的封装
class HttpRequest
{
public:
    enum class Method
    {
        kInvalid,
        kGet,
        kPost,
        kHead,
        kPut,
        kDelete,
        kOptions
    };

    enum class Version
    {
        kUnknown,
        kHttp10,
        kHttp11
    };

    bool setMethod(const char* start, const char* end);

    bool setVersion(std::string_view version);

    void setPath(const char* start, const char* end) { path_.assign(start, end); }

    void setQueryParam(const char* start, const char* end);

    void setReceiveTime(mymuduo::Timestamp time) { receiveTime_ = time; }

    void addHeader(const char* start, const char* colon, const char* end);

    void setBody(std::string body)
    {
        body_ = std::move(body);
        bodyLength_ = body_.size();
    }

    void setBody(const char* start, const char* end)
    {
        if (end > start) {
            body_.assign(start, end - start);
            bodyLength_ = body_.size();
        }
        else {
            body_.clear();
            bodyLength_ = 0;
        }
    }
    void setBodyLength(uint64_t len) { bodyLength_ = len; }

    Method method() const { return method_; }

    Version version() const { return version_; }

    const std::string& path() const { return path_; }

    std::optional<std::string> getQueryParam(std::string_view key) const;

    mymuduo::Timestamp receiveTime() const { return receiveTime_; }

    std::optional<std::string> getHeader(std::string_view field) const;

    std::vector<std::string> getHeaders(std::string_view field) const;

    std::optional<std::string> getCookie(std::string_view name) const;

    const std::string& body() const { return body_; }

    uint64_t bodyLength() const { return bodyLength_; }

    void swap(HttpRequest& other);

private:
    void parseCookies() const;

    // 支持异构查找
    using QueryParam = std::unordered_map<std::string, std::string, TransparentStringHash, std::equal_to<>>;
    // 支持异构查找
    using Header = std::unordered_map<std::string, std::vector<std::string>, CaseInsensitiveStringHash,
                                      CaseInsensitiveStringEqual>;
    // 支持异构查找
    using Cookie = std::unordered_map<std::string, std::string, TransparentStringHash, std::equal_to<>>;

    Method method_ = HttpRequest::Method::kInvalid;      // 请求方法
    Version version_ = HttpRequest::Version::kUnknown;   // http 版本
    std::string path_;                                   // 请求路径
    QueryParam queryParams_;                             // 查询参数
    mymuduo::Timestamp receiveTime_;
    Header headers_;   // 请求头
    mutable bool cookiesParsed_ = false;
    mutable Cookie cookies_;
    std::string body_;   // 请求体
    uint64_t bodyLength_ = 0;
};
}   // namespace mymuduo::http