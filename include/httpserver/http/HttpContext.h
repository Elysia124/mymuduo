#pragma once

#include "httpserver/http/HttpRequest.h"
#include "net/Timestamp.h"
#include <cstddef>

namespace mymuduo {
class Buffer;
}
namespace mymuduo::http {

// 解析请求报文并放入 HttpRequest 对象中
class HttpContext
{
public:
    enum class HttpRequestParseState
    {
        kExpectParseLine,      // 解析请求行
        kExpectParseHeaders,   // 解析请求头
        kExpectParseBody,      // 解析请求体
        kGotAll                // 解析完成
    };

    // 解析报文并封装到 HttpRequest 对象中
    bool parseRequest(Buffer* buf, Timestamp receiveTime);

    bool gotAll() const { return state_ == HttpRequestParseState::kGotAll; }

    void reset()
    {
        headerBytes_ = 0;
        state_ = HttpRequestParseState::kExpectParseLine;
        HttpRequest dummy;
        request_.swap(dummy);
    }

    const HttpRequest& request() const { return request_; }
    HttpRequest& request() { return request_; }

private:
    // 解析请求行
    bool processRequestLine(const char* start, const char* end);
    HttpRequestParseState state_ = HttpRequestParseState::kExpectParseLine;
    HttpRequest request_;

    static constexpr std::size_t kMaxRequestLineSize = 8L * 1024;
    static constexpr std::size_t kMaxHeaderSize = 64L * 1024;
    static constexpr std::size_t kMaxBodySize = 64L * 1024;

    std::size_t headerBytes_ = 0;
};
}   // namespace mymuduo::http