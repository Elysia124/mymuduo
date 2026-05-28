#include "httpserver/session/Session.h"
#include "httpserver/session/SessionManager.h"
#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

using namespace mymuduo::http::session;

Session::Session(std::string sessionId, SessionManager* manager, int maxAge)
    : sessionId_(std::move(sessionId)), sessionManager_(manager), maxAge_(maxAge)
{
    refresh();
}

// 检查 session 是否过期
bool Session::isExpired() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return std::chrono::steady_clock::now() > expiryTime_;
}

void Session::refresh()
{
    std::lock_guard<std::mutex> lock(mutex_);
    expiryTime_ = std::chrono::steady_clock::now() + static_cast<std::chrono::seconds>(maxAge_);
}

void Session::setValue(std::string key, std::string value)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        data_[std::move(key)] = std::move(value);
    }

    if (sessionManager_) {
        sessionManager_->updateSession(shared_from_this());
    }
}

std::optional<std::string> Session::getValue(std::string_view key) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = data_.find(key);
    if (it != data_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Session::remove(std::string_view key)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto it = data_.find(key); it != data_.end()) {
            data_.erase(it);
        }
    }

    if (sessionManager_) {
        sessionManager_->updateSession(shared_from_this());
    }
}
void Session::clear()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.clear();
    }

    if (sessionManager_) {
        sessionManager_->updateSession(shared_from_this());
    }
}