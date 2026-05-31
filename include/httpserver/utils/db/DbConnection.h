#pragma once

#include "httpserver/utils/db/DbConfig.h"
#include "httpserver/utils/db/DbException.h"
#include "net/noncopyable.h"
#include <chrono>
#include <cppconn/connection.h>
#include <cppconn/datatype.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <memory>
#include <mutex>
#include <mysql_driver.h>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace mymuduo::http::db {
class DbQueryResult
{
public:
    DbQueryResult(std::unique_ptr<sql::PreparedStatement> pstmt, std::unique_ptr<sql::ResultSet> result)
        : pstmt_(std::move(pstmt)), result_(std::move(result))
    {}

    DbQueryResult(DbQueryResult&&) noexcept = default;
    DbQueryResult& operator=(DbQueryResult&&) noexcept = default;

    DbQueryResult(const DbQueryResult&) = delete;
    DbQueryResult& operator=(const DbQueryResult&) = delete;

    sql::ResultSet* get() noexcept { return result_.get(); }
    const sql::ResultSet* get() const noexcept { return result_.get(); }

    sql::ResultSet& operator*() { return *result_; }
    const sql::ResultSet& operator*() const { return *result_; }

    sql::ResultSet* operator->() noexcept { return result_.get(); }
    const sql::ResultSet* operator->() const noexcept { return result_.get(); }

    explicit operator bool() const noexcept { return static_cast<bool>(result_); }

private:
    std::unique_ptr<sql::PreparedStatement> pstmt_;   // 预编译语句
    std::unique_ptr<sql::ResultSet> result_;          // 结果集
};

class DbConnection : noncopyable
{
public:
    explicit DbConnection(DbConfig config);
    ~DbConnection();

    bool ping();
    bool isValid();
    void reconnect();
    void cleanupBeforeReturn();
    void close() noexcept;

    void beginTransaction();
    void commit();
    void rollback();

    std::chrono::steady_clock::time_point lastUsedTime() const noexcept { return lastUsed_; }
    void markUsed() noexcept { lastUsed_ = std::chrono::steady_clock::now(); }

    template<typename... Args>
    DbQueryResult executeQuery(const std::string& sqlText, Args&&... args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ensureConnected();

        try {
            auto pstmt = prepare(sqlText);
            bindParams(pstmt.get(), std::forward<Args>(args)...);
            std::unique_ptr<sql::ResultSet> result(pstmt->executeQuery());
            markUsed();
            return {std::move(pstmt), std::move(result)};
        }
        catch (const sql::SQLException& e) {
            throw DbException(buildSqlError("executeQuery failed", sqlText, e));
        }
    }

    template<typename... Args>
    int executeUpdate(const std::string& sqlText, Args&&... args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ensureConnected();

        try {
            auto pstmt = prepare(sqlText);
            bindParams(pstmt.get(), std::forward<Args>(args)...);
            const int affectRows = pstmt->executeUpdate();
            markUsed();
            return affectRows;
        }
        catch (const sql::SQLException& e) {
            throw DbException(buildSqlError("exetuteUpdate failed", sqlText, e));
        }
    }

private:
    template<typename T>
    struct isOptional : std::false_type
    {};

    template<typename T>
    struct isOptional<std::optional<T>> : std::true_type
    {};

    void connect();
    void ensureConnected();
    std::unique_ptr<sql::PreparedStatement> prepare(const std::string& sqlText);
    static std::string buildSqlError(std::string_view prefix, const std::string& sqlText,
                                     const sql::SQLException& e);

    // // 递归终止函数
    // void bindParams(sql::PreparedStatement*, int);

    // template<typename T, typename... Args>
    // void bindParams(sql::PreparedStatement* pstmt, int index, T&& val, Args&&... args)
    // {
    //     bindOne(pstmt, index, val);
    //     bindParams(pstmt, index + 1, std::forward<Args>(args)...);
    // }

    template<typename... Args>
    void bindParams(sql::PreparedStatement* pstmt, Args&&... args)
    {
        int index = 1;
        (bindOne(pstmt, index++, std::forward<Args>(args)), ...);
    }

    // 绑定一个参数至 sql::PreparedStatement
    template<typename T>
    void bindOne(sql::PreparedStatement* pstmt, int index, T&& value)
    {
        using U = std::remove_cvref_t<T>;       // U 是 T 去掉 const volatile & && 后的类型
        if constexpr (isOptional<U>::value) {   // 如果是 std::optional 类型
            if (value.has_value()) {
                bindOne(pstmt, index, value.value());
            }
            else {
                pstmt->setNull(index, sql::DataType::SQLNULL);
            }
        }
        else if constexpr (std::is_same_v<U, std::nullptr_t>) {   // U is nullptr
            pstmt->setNull(index, sql::DataType::SQLNULL);
        }
        else if constexpr (std::is_same_v<U, bool>) {   // U is bool
            pstmt->setBoolean(index, value);
        }
        else if constexpr (std::is_floating_point_v<U>) {   // U is float or double or long double
            pstmt->setDouble(index, value);
        }
        else if constexpr (std::is_integral_v<U>) {   // U is integer
            // Connector/C++ 不同版本的 int64 接口差异较大，用字符串绑定兼容性最好。
            pstmt->setString(index, std::to_string(value));
        }
        else if constexpr (std::is_convertible_v<U, std::string_view>) {
            // U can converted to std::string_view
            pstmt->setString(index, std::string(value));
        }
        else {
            std::ostringstream ss;
            ss << value;
            pstmt->setString(index, ss.str());
        }
    }

    DbConfig config_;
    std::unique_ptr<sql::Connection> conn_;
    mutable std::mutex mutex_;
    std::chrono::steady_clock::time_point lastUsed_ = std::chrono::steady_clock::now();
};
}   // namespace mymuduo::http::db