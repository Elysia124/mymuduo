#pragma once

#include "net/FileUtil.h"
#include "net/noncopyable.h"
#include <cstddef>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <sys/types.h>
namespace mymuduo {

// 负责日志文件滚动
class LogFile : noncopyable
{
public:
    LogFile(std::string basename, off_t rollSize, bool threadSafe = true, int flushInterval = 3,
            int checkEceryN = 1024, bool append = true);

    ~LogFile();

    void append(const char* logline, std::size_t len);
    void flush();

    // 强制滚动文件
    bool rollFile();

    const std::string& basename() const noexcept { return baseneme_; }

private:
    void appendUnlock(const char* logline, std::size_t len);

    static std::string getLogFileName(const std::string& basename, std::time_t* now);

    static constexpr int kRollPerSeconds = 60 * 60 * 24;
    const std::string baseneme_;   // 日志基础文件名
    const off_t rollSize_;         // 日志滚动大小
    const int flushInterval_;      // 定期 flush 间隔，单位秒
    const int checkEveryN_;        // 每写 N 条日志检查一次日期滚动
    const bool append_;

    int count_ = 0;
    std::unique_ptr<std::mutex> mutex_;   // 同步日志时加锁

    std::time_t startOfPeriod_ = 0;
    std::time_t lastRoll_ = 0;
    std::time_t lastFlush_ = 0;

    std::unique_ptr<AppendFile> file_;
};
}   // namespace mymuduo