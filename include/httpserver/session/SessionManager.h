#pragma once

#include "httpserver/session/Session.h"
#include "httpserver/session/SessionStorage.h"
#include "net/noncopyable.h"
#include <memory>
#include <string>
#include <string_view>

namespace mymuduo::http {
class HttpRequest;
class HttpResponse;
}   // namespace mymuduo::http

namespace mymuduo::http::session {
class SessionManager : noncopyable
{
public:
    explicit SessionManager(std::unique_ptr<SessionStorage> storage, int maxAge = 3600,
                            bool secureCookie = false);

    // 普通匿名会话：有就用，没有就建
    std::shared_ptr<Session> getOrCreateSession(const HttpRequest& rep, HttpResponse* resp);
    // 只查已有 session，不创建
    std::shared_ptr<Session> findSession(const HttpRequest& rep, HttpResponse* resp);
    // 登录/提权时使用：始终创建新 sessionId
    std::shared_ptr<Session> createSession(HttpResponse* resp);

    void destroySession(std::string_view sessionId);
    void destroySession(const HttpRequest& rep, HttpResponse* resp);
    void updateSession(std::shared_ptr<Session> session);

private:
    std::string generateSessionId();
    std::string getSessionIdFromCookie(const HttpRequest& req) const;
    void setSessionCookie(std::string sessionId, HttpResponse* resp) const;
    void clearSessionCookie(HttpResponse* resp) const;

    std::unique_ptr<SessionStorage> storage_;
    int maxAge_;

    bool secureCookie_;
};
}   // namespace mymuduo::http::session