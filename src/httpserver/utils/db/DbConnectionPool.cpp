#include "httpserver/utils/db/DbConnectionPool.h"
#include "httpserver/utils/db/DbConfig.h"
#include "httpserver/utils/db/DbConnection.h"
#include "httpserver/utils/db/DbException.h"
#include "net/Logger.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

using namespace mymuduo::http::db;

DbConnectionPool& DbConnectionPool::getInstance()
{
    static DbConnectionPool pool;
    return pool;
}

DbConnectionPool::DbConnectionPool() : state_(std::make_shared<State>()) {}

DbConnectionPool::~DbConnectionPool()
{
    shutdown();
}

void DbConnectionPool::init(DbConfig config)
{
    config = normalizeConfig(std::move(config));
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->initialized) {
            LOG_WARN << "mysql connection pool has already been initialized";
            return;
        }

        if (state_->stopping) {
            state_->stopping = false;
        }
    }

    std::deque<std::unique_ptr<DbConnection>> initialConnections;
    for (std::size_t i = 0; i < config.initialSize; ++i) {
        initialConnections.push_back(makeConnection(config));
    }

    std::size_t initial = 0;
    std::size_t max = 0;
    std::size_t idle = 0;

    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        // 二次检查
        // 避免线程A 和 B 同时通过第一次检查后都进行初始化
        if (state_->initialized) {
            LOG_WARN << "mysql connection pool has already been initialized";
            return;
        }

        state_->config = std::move(config);
        state_->idleConnections = std::move(initialConnections);
        state_->totalConnections = state_->idleConnections.size();
        state_->initialized = true;
        state_->stopping = false;
        state_->healthThread = std::thread(&DbConnectionPool::healthCheckLoop, state_);

        initial = state_->config.initialSize;
        max = state_->config.maxSize;
        idle = state_->idleConnections.size();
    }

    LOG_INFO << "mysql connection pool initialized initial=" << initial << " max=" << max << " idle=" << idle;
}

void DbConnectionPool::init(std::string host, std::string user, std::string password, std::string database,
                            std::size_t initialSize, std::size_t maxSize)
{
    DbConfig config;
    config.host = std::move(host);
    config.user = std::move(user);
    config.password = std::move(password);
    config.database = std::move(database);
    config.initialSize = initialSize;
    config.maxSize = maxSize;

    init(std::move(config));
}

DbConnectionPool::ConnectionPtr DbConnectionPool::getConnection()
{
    std::unique_ptr<DbConnection> owned;
    DbConfig config;
    bool needCreate = false;

    while (true) {
        {
            std::unique_lock<std::mutex> lock(state_->mutex);

            if (!state_->initialized) {
                throw DbException("mysql connection pool is not initialized");
            }

            if (state_->stopping) {
                throw DbException("mysql connection pool is shutting down");
            }

            config = state_->config;

            if (!state_->idleConnections.empty()) {   // 有可取的连接
                owned = std::move(state_->idleConnections.front());
                state_->idleConnections.pop_front();
            }
            else if (state_->totalConnections < state_->config.maxSize) {
                // 没有可取的连接且当前总连接数小于最大连接数
                ++state_->totalConnections;
                needCreate = true;
            }
            else {   // 没有可取的连接且当前总链接数已是最大连接数
                // ok = true：连接池正在关闭或有可用的连接或可以创建新的连接
                // ol = false：在 acquireTimeout 时间内没等到可用连接
                const bool ok = state_->cv.wait_for(lock, state_->config.acquireTimeout, [state = state_] {
                    return state->stopping || !state->idleConnections.empty() ||
                           state->totalConnections < state->config.maxSize;
                });

                if (!ok) {
                    throw DbException("get mysql connection failed");
                }

                continue;
            }
        }

        if (needCreate) {
            try {
                owned = makeConnection(config);
            }
            catch (...) {
                {
                    std::lock_guard<std::mutex> lock(state_->mutex);
                    --state_->totalConnections;
                    state_->cv.notify_one();
                    throw;
                }
            }

            needCreate = false;
        }

        if (!owned) {
            continue;
        }

        try {
            if (config.pingBeforeUse && !owned->ping()) {
                owned->reconnect();
            }

            owned->markUsed();
            return makeHandle(owned.release());
        }
        catch (const std::exception& e) {
            LOG_WARN << "mysql connection checkout failed, recreate it: " << e.what();
            {
                std::lock_guard<std::mutex> lock(state_->mutex);
                --state_->totalConnections;
            }
            state_->cv.notify_one();
        }
    }
}

void DbConnectionPool::shutdown()
{
    std::thread healthThread;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (!state_->initialized && !state_->healthThread.joinable()) {
            return;
        }

        healthThread = std::move(state_->healthThread);
        state_->stopping = true;
    }

    state_->cv.notify_all();
    if (healthThread.joinable()) {
        healthThread.join();
    }

    {
        std::lock_guard<std::mutex> lock(state_->mutex);

        state_->idleConnections.clear();
        state_->totalConnections = 0;
        state_->initialized = false;
    }

    LOG_INFO << "mysql connection pool shutdown";
}

bool DbConnectionPool::initialized() const
{
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->initialized;
}

DbPoolStat DbConnectionPool::stats() const
{
    std::lock_guard<std::mutex> lock(state_->mutex);

    DbPoolStat stats;
    stats.idle = state_->idleConnections.size();
    stats.total = state_->totalConnections;
    stats.active = stats.total >= stats.idle ? stats.total - stats.idle : 0;
    stats.maxSize = state_->config.maxSize;

    return stats;
}

DbConfig DbConnectionPool::normalizeConfig(DbConfig config)
{
    if (config.host.empty()) {
        throw DbException("mysql host is empty");
    }
    if (config.user.empty()) {
        throw DbException("mysql user is empty");
    }
    if (config.database.empty()) {
        throw DbException("mysql database is empty");
    }

    if (config.healthCheckBatchSize == 0) {
        config.healthCheckBatchSize = 1;
    }
    if (config.initialSize == 0) {
        config.initialSize = 1;
    }
    if (config.maxSize == 0) {
        config.maxSize = 1;
    }
    if (config.initialSize > config.maxSize) {
        config.initialSize = config.maxSize;
    }
    if (config.acquireTimeout.count() <= 0) {
        config.acquireTimeout = std::chrono::milliseconds{3000};
    }
    if (config.maxIdleTimeout.count() <= 0) {
        config.maxIdleTimeout = std::chrono::seconds{300};
    }
    if (config.healthCheckInterval.count() <= 0) {
        config.healthCheckInterval = std::chrono::seconds{60};
    }

    return config;
}

std::unique_ptr<DbConnection> DbConnectionPool::makeConnection(const DbConfig& config)
{
    return std::make_unique<DbConnection>(config);
}

void DbConnectionPool::returnConnection(const std::shared_ptr<State>& state, DbConnection* raw) noexcept
{
    if (!raw) {
        return;
    }

    std::unique_ptr<DbConnection> conn(raw);
    if (!conn) {
        return;
    }

    try {
        conn->cleanupBeforeReturn();
    }
    catch (const std::exception& e) {
        LOG_WARN << "mysql connection cleanup failed when return: " << e.what();
    }

    bool shouldDestroy = false;
    {
        std::lock_guard<std::mutex> lock(state->mutex);

        shouldDestroy = state->stopping || !state->initialized || !conn->isValid();
        if (!shouldDestroy) {
            conn->markUsed();
            state->idleConnections.push_back(std::move(conn));
        }
        else if (state->totalConnections > 0) {
            --state->totalConnections;
        }
    }

    state->cv.notify_one();
}

void DbConnectionPool::healthCheckLoop(std::shared_ptr<State> state)
{
    while (true) {
        DbConfig config;
        std::vector<std::unique_ptr<DbConnection>> checking;
        std::size_t shrinkBudget = 0;

        {
            std::unique_lock<std::mutex> lock(state->mutex);

            const bool stopped = state->cv.wait_for(
                lock, state->config.healthCheckInterval, [state] { return state->stopping; });

            if (stopped) {
                return;
            }

            if (!state->initialized) {
                continue;
            }

            config = state->config;

            if (state->totalConnections > config.initialSize) {
                shrinkBudget = state->totalConnections - config.initialSize;
            }

            const std::size_t n = std::min(config.healthCheckBatchSize, state->idleConnections.size());

            for (std::size_t i = 0; i < n; ++i) {
                // 每次从 idleConnections 队头拿，因为检查过的连接新加入在队尾
                // 可以保证每次检查的连接都是没检查过的连接
                checking.push_back(std::move(state->idleConnections.front()));
                state->idleConnections.pop_front();
            }
        }

        std::vector<std::unique_ptr<DbConnection>> alive;
        alive.reserve(checking.size());

        std::size_t destroyed = 0;
        const auto now = std::chrono::steady_clock::now();

        for (auto& conn : checking) {
            if (!conn) {
                ++destroyed;
                continue;
            }

            const bool idleTooLong = now - conn->lastUsedTime() >= config.maxIdleTimeout;

            if (idleTooLong && shrinkBudget > 0) {
                --shrinkBudget;
                ++destroyed;
                continue;
            }

            if (!conn->ping()) {
                try {
                    conn->reconnect();
                }
                catch (const std::exception& e) {
                    LOG_WARN << "mysql reconnect failed in health check: " << e.what();
                    ++destroyed;
                    continue;
                }
            }

            alive.push_back(std::move(conn));
        }

        {
            std::lock_guard<std::mutex> lock(state->mutex);

            if (!state->stopping && state->initialized) {
                for (auto& conn : alive) {
                    if (conn) {
                        // 不要用state->idleConnectionss = std::move(alive); 或
                        // state->idleConnectionss.swap(alive);
                        // 因为可能其他线程已经归还了一些连接
                        state->idleConnections.push_back(std::move(conn));
                    }
                }
            }
            else {
                destroyed += alive.size();
            }

            if (destroyed > 0) {
                state->totalConnections =
                    state->totalConnections >= destroyed ? state->totalConnections - destroyed : 0;
            }
        }

        state->cv.notify_all();
    }
}

DbConnectionPool::ConnectionPtr DbConnectionPool::makeHandle(DbConnection* conn)
{
    std::weak_ptr<State> weakState(state_);

    return {conn, [weakState](DbConnection* raw) {
                if (!raw) {
                    return;
                }
                if (auto state = weakState.lock()) {
                    // 如果连接池对象还活着，就归还连接
                    returnConnection(state, raw);
                }
                else {   // 否则直接删除
                    delete raw;
                }
            }};
}