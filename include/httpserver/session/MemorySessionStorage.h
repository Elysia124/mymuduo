#pragma once

#include "httpserver/session/SessionStorage.h"
#include "httpserver/utils/StringHash.h"
#include "net/TimerId.h"
#include "net/noncopyable.h"
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace mymuduo {
class EventLoop;
}

namespace mymuduo::http::session {
class Session;
class MemorySessionStorage : public SessionStorage, noncopyable
{
public:
    ~MemorySessionStorage() override;

    void save(std::shared_ptr<Session> session) override;
    std::shared_ptr<Session> load(std::string_view sessionId) override;
    void remove(std::string_view sessionId) override;
    void setClearExpiredRegularly(EventLoop* loop);

private:
    using Sessions = std::unordered_map<std::string, std::shared_ptr<Session>,
                                        mymuduo::http::TransparentStringHash, std::equal_to<>>;

    struct State
    {
        Sessions sessions;
        std::mutex mutex;
    };

    static void clearExpired(const std::shared_ptr<State>& state);

    EventLoop* loop_ = nullptr;
    TimerId clearTimer_;
    std::shared_ptr<State> state_ = std::make_shared<State>();
};
}   // namespace mymuduo::http::session