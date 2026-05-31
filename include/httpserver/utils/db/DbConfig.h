#pragma once

#include <chrono>
#include <cstddef>
#include <string>
namespace mymuduo::http::db {
struct DbConfig
{
    std::string host = "tcp://127.0.0.1:3307";   // tcp://ip:port
    std::string user;
    std::string password;
    std::string database;
    std::string charset = "utf8mb4";

    std::size_t initialSize = 4;                      // 启动时预创建的连接数(默认为IO线程数)
    std::size_t maxSize = 16;                         // 高峰时允许创建的最大连接数
    std::size_t healthCheckBatchSize = 4;             // 每次健康检查取出的连接数
    std::chrono::milliseconds acquireTimeout{3000};   // 连接池已满时，IO线程默认最大等待时间(毫秒)

    // 空闲连接超时(当空闲连接超过该时间且当前总连接数大于 initialSize 时，该连接可以被回收)
    std::chrono::seconds maxIdleTimeout{300};
    std::chrono::seconds healthCheckInterval{60};   // 后台健康检查时间间隔
    bool pingBeforeUse = false;   // 取出连接时是否先 ping 一次(若为 true 则多一次 SELECT 1)
};

struct DbPoolStat
{
    std::size_t total = 0;
    std::size_t idle = 0;
    std::size_t active = 0;
    std::size_t maxSize = 0;
};
}   // namespace mymuduo::http::db