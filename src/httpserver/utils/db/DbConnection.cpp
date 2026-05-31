#include "httpserver/utils/db/DbConnection.h"
#include "httpserver/utils/db/DbException.h"
#include "net/Logger.h"
#include <cppconn/connection.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <exception>
#include <memory>
#include <mutex>
#include <mysql_driver.h>
#include <string>
#include <utility>

using namespace mymuduo::http::db;

DbConnection::DbConnection(DbConfig config) : config_(std::move(config))
{
    connect();
}

DbConnection::~DbConnection()
{
    close();
}


bool DbConnection::ping()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!conn_ || conn_->isClosed()) {
        return false;
    }


    try {
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        if (!stmt) {
            return false;
        }
        std::unique_ptr<sql::ResultSet> result(stmt->executeQuery("SELECT 1"));
        return result && result->next();
    }
    catch (const sql::SQLException& e) {
        LOG_WARN << "mysql ping failed: " << e.what();
        return false;
    }
    catch (const std::exception& e) {
        LOG_WARN << "mysql ping failed: " << e.what();
    }

    return false;
}

bool DbConnection::isValid()
{
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        return conn_ && !conn_->isClosed();
    }
    catch (const sql::SQLException&) {
        return false;
    }
}

void DbConnection::reconnect()
{
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        close();
        connect();
    }
    catch (const DbException&) {
        throw;
    }
    catch (const std::exception& e) {
        throw DbException(std::string("mysql reconnect failed: ") + e.what());
    }
}

void DbConnection::cleanupBeforeReturn()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!conn_) {
        return;
    }

    try {
        if (!conn_->getAutoCommit()) {   // 如果为 getAutoCommit() 为 false，则说明手动开启了一个事务
            conn_->rollback();           // 回滚
            conn_->setAutoCommit(true);   // 恢复为默认设置
        }
    }
    catch (const sql::SQLException& e) {
        LOG_WARN << "mysql cleanup before return failed" << e.what();
        close();
    }
}

void DbConnection::close() noexcept
{
    try {
        if (conn_ && !conn_->isClosed()) {
            conn_->close();
        }
    }
    catch (...) {
    }

    conn_.reset();
}

void DbConnection::beginTransaction()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    try {
        conn_->setAutoCommit(false);
    }
    catch (const sql::SQLException& e) {
        throw DbException(buildSqlError("begin transaction failed", "", e));
    }
}

void DbConnection::commit()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    try {
        conn_->commit();
        conn_->setAutoCommit(true);
        markUsed();
    }
    catch (const sql::SQLException& e) {
        throw DbException(buildSqlError("commit failed", "", e));
    }
}

void DbConnection::rollback()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ensureConnected();

    try {
        conn_->rollback();
        conn_->setAutoCommit(true);
        markUsed();
    }
    catch (const sql::SQLException& e) {
        throw DbException(buildSqlError("rollback failed", "", e));
    }
}

void DbConnection::connect()
{
    try {
        auto* driver = sql::mysql::get_mysql_driver_instance();
        std::unique_ptr<sql::Connection> newConn(
            driver->connect(config_.host, config_.user, config_.password));
        if (!newConn) {
            throw DbException("mysql driver returned null connection");
        }

        newConn->setSchema(config_.database);

        if (!config_.charset.empty()) {
            std::unique_ptr<sql::Statement> stmt(newConn->createStatement());
            stmt->execute("SET NAMES " + config_.charset);
        }

        conn_ = std::move(newConn);
        markUsed();
        LOG_INFO << "mysql connection established host=" << config_.host.c_str()
                 << " database=" << config_.database.c_str();
    }
    catch (const sql::SQLException& e) {
        throw DbException(buildSqlError("create mysql connection failed", "", e));
    }
}

void DbConnection::ensureConnected()
{
    if (!conn_ || conn_->isClosed()) {
        connect();
    }
}

std::unique_ptr<sql::PreparedStatement> DbConnection::prepare(const std::string& sqlText)
{
    return std::unique_ptr<sql::PreparedStatement>(conn_->prepareStatement(sqlText));
}

std::string DbConnection::buildSqlError(std::string_view prefix, const std::string& sqlText,
                                        const sql::SQLException& e)
{
    std::string message(prefix);
    message += ": ";
    message += e.what();
    message += ", errorCode=" + std::to_string(e.getErrorCode());
    message += ", sqlState=";
    message += e.getSQLState();

    if (!sqlText.empty()) {
        message += ", sql=";
        message += sqlText;
    }

    return message;
}
