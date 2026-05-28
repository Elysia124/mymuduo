#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mymuduo {
class Buffer;
}
namespace mymuduo::http {

// 响应报文
class HttpResponse
{
public:
    enum class HttpStatusCode
    {
        kUnknown,
        k200Ok = 200,
        k204NoContent = 204,
        k301MovedPermanently = 301,
        k302MovedTemporarily = 302,
        k400BadRequest = 400,
        k401Unauthorized = 401,
        k403Forbidden = 403,
        k404NotFound = 404,
        k409Conflict = 409,
        k500InternalServerError = 500
    };

    explicit HttpResponse(bool close = true) : closeConnection_(close){};

    void setHttpVersion(std::string version) { httpVersion_ = std::move(version); }
    void setStatusCode(HttpStatusCode code) { statusCode_ = code; }
    void setStatusMessage(std::string message) { statusMessage_ = std::move(message); }
    void setCloseConnection(bool on) { closeConnection_ = on; }
    void setContentType(std::string contentType) { setHeader("Content-Type", std::move(contentType)); }
    void appendHeader(std::string key, std::string value);
    void setHeader(std::string key, std::string value);
    void setCookie(std::string name, std::string value, int maxAge, bool httpOnly = true,
                   bool secureCookie = false);
    void clearCookie(std::string name, bool secureCookie = false);
    void setBody(std::string body) { body_ = std::move(body); }
    void appendToBuffer(Buffer* buffer, bool sendBody = true) const;
    bool closeConnection() const { return closeConnection_; }


private:
    static bool isManagedHeader(std::string_view key);
    void applyManagedHeader(std::string_view key, std::string_view value);

    std::string httpVersion_ = "HTTP/1.1";
    HttpStatusCode statusCode_ = HttpStatusCode::k200Ok;
    std::string statusMessage_ = "OK";
    bool closeConnection_;
    std::vector<std::pair<std::string, std::string>> headers_;
    std::string body_;
    bool isFile_ = false; 
};
}   // namespace mymuduo::http
