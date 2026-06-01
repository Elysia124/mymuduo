#pragma once

#include "net/LogStream.h"
#include "net/noncopyable.h"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <sys/types.h>
#include <vector>
namespace mymuduo {
class AsyncLogging : noncopyable
{
public:
    AsyncLogging(std::string basename, off_t rollSize, bool toStdout, int flushInterval = 3,
                 int maxQueuedBuffers = 25, bool append = true);
    ~AsyncLogging();

    void start();
    void stop();

    void append(const char* logline, std::size_t len);

    void flush();

    bool running() const { return running_.load(std::memory_order_acquire); }

    std::uint64_t droppedLogs() const { return droppedLogs_.load(std::memory_order_relaxed); }

private:
    using Buffer = detail::FixedBuffer<4 * 1024 * 1024>;
    using BufferPtr = std::unique_ptr<Buffer>;
    using BufferVector = std::vector<BufferPtr>;

    void threadFunc();

    const int flushInterval_;
    const int maxQueueBuffers_;
    const std::string basename_;
    const off_t rollSize_;
    const bool toStdout_;
    const bool append_;

    std::atomic<bool> running_{false};
    std::thread thread_;

    std::mutex mutex_;
    std::condition_variable cond_;        // 通知后台线程处理日志
    std::condition_variable flushCond_;   // 通知 flush 完成


    BufferPtr currentBuffer_;   // 前端线程写入的 buffer
    BufferPtr nextBuffer_;      // 备用 buffer
    BufferVector buffers_;      // 已经写满或等待后台线程写入的 buffer 列表

    bool flushRequested_ = false;   // 强制 flush 标志
    bool flushDone_ = false;        // flush 完成标志
    std::atomic<std::uint64_t> droppedLogs_{0};
};
}   // namespace mymuduo