#pragma once

#include "httpserver/utils/db/DbConfig.h"
#include "httpserver/utils/db/DbConnection.h"
#include "net/noncopyable.h"
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
namespace mymuduo::http::db {
class DbConnectionPool : noncopyable
{
public:
    using ConnectionPtr = std::unique_ptr<DbConnection, std::function<void(DbConnection*)>>;

    // 返回唯一实例
    static DbConnectionPool& getInstance();

    void init(DbConfig config);
    void init(std::string host, std::string user, std::string password, std::string database,
              std::size_t initialSize = 4, std::size_t maxSize = 16);

    ConnectionPtr getConnection();

    void shutdown();

    bool initialized() const;
    DbPoolStat stats() const;

private:
    DbConnectionPool();
    ~DbConnectionPool();

    struct State
    {
        DbConfig config;
        std::deque<std::unique_ptr<DbConnection>> idleConnections;   // 空闲连接队列
        std::size_t totalConnections = 0;
        bool initialized = false;
        bool stopping = false;
        mutable std::mutex mutex;
        std::condition_variable cv;
        std::thread healthThread;
    };

    static DbConfig normalizeConfig(DbConfig config);
    static std::unique_ptr<DbConnection> makeConnection(const DbConfig& config);
    static void returnConnection(const std::shared_ptr<State>& state, DbConnection* raw) noexcept;
    static void healthCheckLoop(std::shared_ptr<State> state);

    ConnectionPtr makeHandle(DbConnection* conn);
    std::shared_ptr<State> state_;
};
}   // namespace mymuduo::http::db