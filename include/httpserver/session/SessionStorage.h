#pragma once

#include <memory>
#include <string>
#include <string_view>
namespace mymuduo::http::session {
class Session;
class SessionStorage
{
public:
    virtual ~SessionStorage() = default;
    virtual void save(std::shared_ptr<Session> session) = 0;
    virtual std::shared_ptr<Session> load(std::string_view sessionId) = 0;
    virtual void remove(std::string_view sessionId) = 0;
};
}   // namespace mymuduo::http::session