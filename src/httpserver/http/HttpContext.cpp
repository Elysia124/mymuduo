#include "httpserver/http/HttpContext.h"
#include "httpserver/http/HttpRequest.h"
#include "net/Buffer.h"
#include "net/Logger.h"
#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

using namespace mymuduo::http;

namespace {
bool parseContentLength(std::string_view s, uint64_t& contentLength)
{
    if (s.empty()) {
        return false;
    }

    uint64_t result = 0;
    if (auto [ptr, ec] = std::from_chars(s.begin(), s.data() + s.size(), result);
        ec == std::errc() && ptr == s.data() + s.size()) {
        contentLength = result;
        return true;
    }

    return false;
}
}   // namespace

bool HttpContext::parseRequest(Buffer* buf, Timestamp receiveTime)
{
    bool ok = true;
    bool hasMore = true;

    while (hasMore) {
        switch (state_) {
            case HttpRequestParseState::kExpectParseLine:
            {
                const char* crlf = buf->findCRLF();
                if (crlf) {
                    ok = processRequestLine(buf->peek(), crlf);
                    if (ok) {
                        request_.setReceiveTime(receiveTime);
                        buf->retrieveUntil(crlf + 2);
                        state_ = HttpRequestParseState::kExpectParseHeaders;
                    }
                    else {
                        LOG_ERROR << "HTTP: parse request line failed";
                        hasMore = false;
                    }
                }
                else {
                    if (buf->readableBytes() > kMaxRequestLineSize) {
                        LOG_ERROR << "HTTP: current request line size has exceeded the max size";
                        ok = false;
                    }
                    hasMore = false;
                }
                break;
            }
            case HttpRequestParseState::kExpectParseHeaders:
            {
                const char* crlf = buf->findCRLF();
                if (!crlf) {
                    if (headerBytes_ + buf->readableBytes() > kMaxHeaderSize) {
                        LOG_ERROR << "HTTP: current request header size has exceeded the max size";
                        ok = false;
                    }
                    hasMore = false;
                    break;
                }

                const char* lineStart = buf->peek();
                const char* colon = std::find(lineStart, crlf, ':');

                if (lineStart == crlf) {   // 空行，headers 结束
                    buf->retrieveUntil(crlf + 2);

                    if (request_.getHeader("Transfer-Encoding").has_value()) {
                        LOG_ERROR << "HTTP: Transfer-Encoding is not supported";
                        ok = false;
                        hasMore = false;
                        break;
                    }

                    auto contentLengthHeader = request_.getHeader("Content-Length");

                    if (contentLengthHeader.has_value()) {
                        uint64_t bodyLength = 0;

                        if (!parseContentLength(contentLengthHeader.value(), bodyLength)) {
                            LOG_ERROR << "HTTP: bad Content-Length";
                            ok = false;
                            hasMore = false;
                            break;
                        }

                        if (bodyLength > kMaxBodySize) {
                            LOG_ERROR << "HTTP: Content-Length has exceeded the max";
                            ok = false;
                            hasMore = false;
                            break;
                        }

                        request_.setBodyLength(bodyLength);

                        if (bodyLength > 0) {
                            state_ = HttpRequestParseState::kExpectParseBody;
                        }
                        else {
                            state_ = HttpRequestParseState::kGotAll;
                            hasMore = false;
                        }
                    }
                    else {
                        state_ = HttpRequestParseState::kGotAll;
                        hasMore = false;
                    }
                }
                else if (colon != crlf) {   // 正常 header
                    const char* lineStart = buf->peek();

                    // 当前一行 header 的大小
                    const auto lineSize = static_cast<std::size_t>(crlf + 2 - lineStart);
                    if (headerBytes_ + lineSize > kMaxHeaderSize) {
                        LOG_ERROR << "HTTP: request header size has exceeded the max";
                        ok = false;
                        hasMore = false;
                        break;
                    }
                    headerBytes_ += lineSize;

                    request_.addHeader(lineStart, colon, crlf);
                    buf->retrieveUntil(crlf + 2);
                }
                else {   // 非法 header 行，没有 ':'
                    LOG_ERROR << "HTTP: bad request header(no ':')";
                    ok = false;
                    hasMore = false;
                }
                break;
            }
            case HttpRequestParseState::kExpectParseBody:
            {
                if (buf->readableBytes() < request_.bodyLength()) {
                    hasMore = false;
                    return true;   // wait for more data
                }

                std::string body(buf->peek(), buf->peek() + request_.bodyLength());
                request_.setBody(std::move(body));

                buf->retrieve(request_.bodyLength());
                state_ = HttpRequestParseState::kGotAll;
                hasMore = false;
                break;
            }
            case HttpRequestParseState::kGotAll:
            {
                hasMore = false;
                break;
            }
        }
    }
    return ok;
}

bool HttpContext::processRequestLine(const char* start, const char* end)
{
    bool succeed = false;
    const char* begin = start;
    const char* space = std::find(begin, end, ' ');   // find the first space

    if (space != end && request_.setMethod(begin, space)) {   // set http method
        begin = space + 1;
        space = std::find(begin, end, ' ');
        if (space != end) {
            const char* questionPos = std::find(begin, space, '?');
            if (questionPos != space) {   // if path has query parameters
                request_.setPath(begin, questionPos);
                request_.setQueryParam(questionPos + 1, space);
            }
            else {
                request_.setPath(begin, space);
            }

            begin = space + 1;
            std::string_view version(begin, end);
            succeed = request_.setVersion(version);
        }
    }
    return succeed;
}