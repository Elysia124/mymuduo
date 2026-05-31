#pragma once

#include "httpserver/utils/db/DbConfig.h"
#include "httpserver/utils/db/DbConnection.h"
#include "httpserver/utils/db/DbConnectionPool.h"
#include <cppconn/resultset.h>
#include <cstddef>
#include <utility>
namespace mymuduo::http {
class MysqlUtil
{
public:
    using Config = db::DbConfig;
    using Stats = db::DbPoolStat;
    using ConnectionPtr = db::DbConnectionPool::ConnectionPtr;

    class PooledQueryResult
    {
    public:
        PooledQueryResult(ConnectionPtr conn, db::DbQueryResult result)
            : conn_(std::move(conn)), result_(std::move(result))
        {}

        PooledQueryResult(const PooledQueryResult&) = delete;
        PooledQueryResult& operator=(const PooledQueryResult&) = delete;

        PooledQueryResult(PooledQueryResult&&) = default;
        PooledQueryResult& operator=(PooledQueryResult&&) = default;

        sql::ResultSet* get() noexcept { return result_.get(); }
        const sql::ResultSet* get() const noexcept { return result_.get(); }

        sql::ResultSet* operator->() noexcept { return result_.get(); }
        const sql::ResultSet* operator->() const noexcept { return result_.get(); }

        sql::ResultSet& operator*() { return *result_; }
        const sql::ResultSet& operator*() const { return *result_; }

        explicit operator bool() const noexcept { return static_cast<bool>(result_); }

    private:
        // 查询结果存在期间，连接不能归还，避免 resultset 遍历时连接被其他线程复用
        ConnectionPtr conn_;
        db::DbQueryResult result_;
    };

    static void init(Config config) { db::DbConnectionPool::getInstance().init(std::move(config)); }

    static void init(std::string host, std::string user, std::string password, std::string database,
                     std::size_t initialSize = 4, std::size_t maxSize = 16)
    {
        db::DbConnectionPool::getInstance().init(
            std::move(host), std::move(user), std::move(password), std::move(database), initialSize, maxSize);
    }

    static ConnectionPtr getConnection() { return db::DbConnectionPool::getInstance().getConnection(); }

    template<typename... Args>
    static PooledQueryResult executeQuery(const std::string& sqlText, Args&&... args)
    {
        auto conn = getConnection();
        auto result = conn->executeQuery(sqlText, std::forward<Args>(args)...);
        return {std::move(conn), std::move(result)};
    }

    template<typename... Args>
    static int executeUpdate(const std::string& sqlText, Args&&... args)
    {
        auto conn = getConnection();
        return conn->executeUpdate(sqlText, std::forward<Args>(args)...);
    }

    template<typename Func, typename... Args>
    static decltype(auto) query(const std::string& sqlText, Func&& func, Args&&... args)
    {
        auto conn = getConnection();
        auto result = conn->executeQuery(sqlText, std::forward<Args>(args)...);
        return std::forward<Func>(func)(*result);
    }

    static Stats stats() { return db::DbConnectionPool::getInstance().stats(); }

    static void shutdown() { db::DbConnectionPool::getInstance().shutdown(); }
};
}   // namespace mymuduo::http