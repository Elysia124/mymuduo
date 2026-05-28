#pragma once

#include "httpserver/utils/StringHash.h"
#include "net/noncopyable.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace mymuduo::http::session {
class SessionManager;
class Session : noncopyable, public std::enable_shared_from_this<Session>
{
public:
    Session(std::string sessionId, SessionManager* manager, int maxAge = 3600);

    const std::string& id() const { return sessionId_; }
    bool isExpired() const;
    void refresh();
    SessionManager* sessionManager() const { return sessionManager_; }

    void setValue(std::string key, std::string value);
    std::optional<std::string> getValue(std::string_view key) const;
    void remove(std::string_view key);
    void clear();

private:
    std::string sessionId_;
    std::unordered_map<std::string, std::string, mymuduo::http::TransparentStringHash, std::equal_to<>> data_;   // 存储用户状态
    std::chrono::steady_clock::time_point expiryTime_;    // 服务端 session 过期时间
    SessionManager* const sessionManager_;
    int maxAge_;   // 过期时间(秒)
    mutable std::mutex mutex_;
};
}   // namespace mymuduo::http::session