#include "httpserver/session/MemorySessionStorage.h"
#include "httpserver/session/Session.h"
#include "net/EventLoop.h"
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace mymuduo::http::session;

MemorySessionStorage::~MemorySessionStorage()
{
    if (loop_ && clearTimer_.valid()) {
        loop_->cancel(clearTimer_);
    }
}

void MemorySessionStorage::save(std::shared_ptr<Session> session)
{
    if (!session) {
        return;
    }

    auto state = state_;
    std::lock_guard<std::mutex> lock(state->mutex);
    state->sessions[session->id()] = std::move(session);
}

std::shared_ptr<Session> MemorySessionStorage::load(std::string_view sessionId)
{
    auto state = state_;
    std::lock_guard<std::mutex> lock(state->mutex);

    auto it = state->sessions.find(sessionId);
    if (it == state->sessions.end()) {
        return nullptr;
    }

    if (it->second->isExpired()) {
        state->sessions.erase(it);
        return nullptr;
    }

    return it->second;
}

void MemorySessionStorage::remove(std::string_view sessionId)
{
    auto state = state_;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (auto it = state->sessions.find(sessionId); it != state->sessions.end()) {
        state->sessions.erase(it);
    }
}

// 定时清理过期 session
void MemorySessionStorage::setClearExpiredRegularly(mymuduo::EventLoop* loop)
{
    if (!loop) {
        return;
    }

    if (loop_ && clearTimer_.valid()) {
        loop_->cancel(clearTimer_);
    }

    loop_ = loop;
    std::weak_ptr<State> weakState(state_);
    clearTimer_ = loop_->runEvery(60.0, [weakState] {
        if (auto state = weakState.lock()) {
            clearExpired(state);
        }
    });
}

void MemorySessionStorage::clearExpired(const std::shared_ptr<State>& state)
{
    std::vector<std::shared_ptr<Session>> toDestroy;

    {
        std::lock_guard<std::mutex> lock(state->mutex);

        for (auto it = state->sessions.begin(); it != state->sessions.end();) {
            if (it->second->isExpired()) {
                toDestroy.push_back(std::move(it->second));
                it = state->sessions.erase(it);
            }
            else {
                ++it;
            }
        }
    }
}