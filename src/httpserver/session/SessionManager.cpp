#include "httpserver/session/SessionManager.h"
#include "httpserver/http/HttpRequest.h"
#include "httpserver/http/HttpResponse.h"
#include "net/Logger.h"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <sys/random.h>
#include <sys/types.h>
#include <utility>

namespace {
std::unique_ptr<mymuduo::http::session::SessionStorage> checkNotNull(
    std::unique_ptr<mymuduo::http::session::SessionStorage> storage)
{
    if (!storage) {
        LOG_FATAL << "SessionStorage is nullptr";
    }
    return storage;
}

void fillRandomBytes(unsigned char* data, std::size_t len)
{
    std::size_t offset = 0;
    while (offset < len) {
        ssize_t n = ::getrandom(data + offset, len - offset, 0);
        if (n > 0) {
            offset += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }

        LOG_FATAL << "getrandom failed, errno=" << errno << " error=" << strerror(errno);
    }
}
}   // namespace
using namespace mymuduo::http::session;

SessionManager::SessionManager(std::unique_ptr<SessionStorage> storage, int maxAge, bool secureCookie)
    : storage_(checkNotNull(std::move(storage))), maxAge_(maxAge), secureCookie_(secureCookie)
{}

// get a sessionId from HttpRequest if HttpRequest has it
// or create a new session
std::shared_ptr<Session> SessionManager::getOrCreateSession(const HttpRequest& rep, HttpResponse* resp)
{
    std::string sessionId = getSessionIdFromCookie(rep);
    std::shared_ptr<Session> session;

    if (!sessionId.empty()) {
        session = storage_->load(sessionId);
    }

    if (!session) {
        sessionId = generateSessionId();
        while (storage_->load(sessionId)) {   // 如果 sessionId 已存在，就重新生成
            sessionId = generateSessionId();
        }
        session = std::make_shared<Session>(sessionId, this, maxAge_);
        setSessionCookie(sessionId, resp);
    }
    else {
        session->refresh();
        setSessionCookie(session->id(), resp);
    }

    storage_->save(session);
    return session;
}

std::shared_ptr<Session> SessionManager::findSession(const HttpRequest& rep, HttpResponse* resp)
{
    std::string sessionId = getSessionIdFromCookie(rep);

    if (sessionId.empty()) {
        return nullptr;
    }

    std::shared_ptr<Session> session;
    session = storage_->load(sessionId);
    if (!session) {
        clearSessionCookie(resp);
        return nullptr;
    }

    session->refresh();
    setSessionCookie(session->id(), resp);
    storage_->save(session);
    return session;
}

std::shared_ptr<Session> SessionManager::createSession(HttpResponse* resp)
{
    std::string sessionId = generateSessionId();
    while (storage_->load(sessionId)) { sessionId = generateSessionId(); }

    auto session = std::make_shared<Session>(sessionId, this, maxAge_);
    storage_->save(session);
    setSessionCookie(sessionId, resp);
    return session;
}

void SessionManager::destroySession(std::string_view sessionId)
{
    storage_->remove(sessionId);
}

void SessionManager::destroySession(const HttpRequest& rep, HttpResponse* resp)
{
    std::string sessionId = getSessionIdFromCookie(rep);
    if (!sessionId.empty()) {
        storage_->remove(sessionId);
    }

    clearSessionCookie(resp);
}

void SessionManager::updateSession(std::shared_ptr<Session> session)
{
    storage_->save(std::move(session));
}

// 生成唯一的会话标识符
std::string SessionManager::generateSessionId()
{
    std::array<unsigned char, 16> buf;
    fillRandomBytes(buf.data(), buf.size());

    std::string sessionId;
    sessionId.reserve(buf.size() * 2);
    constexpr char hexChars[] = "0123456789abcdef";

    for (unsigned char c : buf) {
        // 一个unsigned char 8字节中的高4字节转为一个16进制字符
        sessionId.push_back(hexChars[(c >> 4) & 0x0F]);

        // 一个unsigned char 8字节中的低4字节转为一个16进制字符
        sessionId.push_back(hexChars[c & 0x0F]);
    }
    return sessionId;
}

std::string SessionManager::getSessionIdFromCookie(const HttpRequest& req) const
{
    return req.getCookie("sessionId").value_or("");
}

void SessionManager::setSessionCookie(std::string sessionId, HttpResponse* resp) const
{
    if (!resp) {
        return;
    }
    resp->setCookie("sessionId", std::move(sessionId), maxAge_, true, secureCookie_);
}

void SessionManager::clearSessionCookie(HttpResponse* resp) const
{
    if (!resp) {
        return;
    }

    resp->clearCookie("sessionId", secureCookie_);
}