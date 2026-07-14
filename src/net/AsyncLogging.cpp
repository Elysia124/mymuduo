#include "net/AsyncLogging.h"
#include "net/LogFile.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <utility>

using namespace mymuduo;

AsyncLogging::AsyncLogging(std::string basename, off_t rollSize, bool toStdout, int flushInterval,
                           int maxQueuedBuffers, bool append)
    : flushInterval_(flushInterval > 0 ? flushInterval : 4)
    , maxQueueBuffers_(maxQueuedBuffers > 0 ? maxQueuedBuffers : 2)
    , basename_(std::move(basename))
    , rollSize_(rollSize)
    , toStdout_(toStdout)
    , append_(append)
    , currentBuffer_(std::make_unique<Buffer>())
    , nextBuffer_(std::make_unique<Buffer>())
{
    buffers_.reserve(16);
}

AsyncLogging::~AsyncLogging()
{
    if (running()) {
        stop();
    }
}

void AsyncLogging::start()
{
    bool expected = false;

    // aviod restart
    if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    thread_ = std::thread([this] { threadFunc(); });
}

void AsyncLogging::stop()
{
    bool expected = true;

    // avoid restop
    if (!running_.compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        return;
    }

    cond_.notify_one();

    if (thread_.joinable()) {
        thread_.join();
    }
}

void AsyncLogging::append(const char* logline, std::size_t len)
{
    if (!logline || len == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // currentBuffer_ has enough space
    if (static_cast<std::size_t>(currentBuffer_->avail()) >= len) {
        currentBuffer_->append(logline, len);
        return;
    }

    // the nums of buffer has reached the limit
    if (static_cast<int>(buffers_.size()) >= maxQueueBuffers_) {
        droppedLogs_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    buffers_.push_back(std::move(currentBuffer_));

    if (nextBuffer_) {
        currentBuffer_ = std::move(nextBuffer_);
    }
    else {
        currentBuffer_ = std::make_unique<Buffer>();
    }

    currentBuffer_->append(logline, len);

    cond_.notify_one();
}

void AsyncLogging::flush()
{
    if (!running()) {
        return;
    }

    std::unique_lock<std::mutex> lock(mutex_);

    flushRequested_ = true;   // 标志是强制 flush 唤醒
    flushDone_ = false;

    // 通知后台线程
    cond_.notify_one();

    // 等待 flush 完成
    flushCond_.wait(lock, [this] { return flushDone_; });
}

void AsyncLogging::threadFunc()
{
    std::unique_ptr<LogFile> output;
    if (!basename_.empty()) {
        output = std::make_unique<LogFile>(basename_,
                                           rollSize_,
                                           false,   // 只有后台线程写文件，不需要 LogFile 内部加锁
                                           flushInterval_,
                                           1024,
                                           append_);
    }

    BufferPtr newBuffer1 = std::make_unique<Buffer>();
    BufferPtr newBuffer2 = std::make_unique<Buffer>();
    BufferVector bufferToWrite;
    bufferToWrite.reserve(16);

    while (true) {
        bool notifyFlush = false;   // 是否是 flush

        {
            std::unique_lock<std::mutex> lock(mutex_);

            // 如果没日志可写且不需要强制 flush, 则继续等待
            if (buffers_.empty() && currentBuffer_->length() == 0 && running() && !flushRequested_) {
                cond_.wait_for(lock, std::chrono::seconds(flushInterval_));
            }

            // 如果程序退出且没日志可写且不需要强制 flush，则直接退出
            if (!running() && buffers_.empty() && currentBuffer_->length() == 0 && !flushRequested_) {
                return;
            }

            // 如果有日志可写
            if (currentBuffer_->length() > 0) {
                buffers_.push_back(std::move(currentBuffer_));

                if (newBuffer1) {
                    currentBuffer_ = std::move(newBuffer1);
                }
                else {
                    currentBuffer_ = std::make_unique<Buffer>();
                }
            }

            // 锁内只交换 buffers，锁外写入
            bufferToWrite.swap(buffers_);

            // 补充 nextBuffer_
            if (!nextBuffer_) {
                if (newBuffer2) {
                    nextBuffer_ = std::move(newBuffer2);
                }
                else {
                    nextBuffer_ = std::make_unique<Buffer>();
                }
            }

            // 如果是由强制 flush 唤醒
            if (flushRequested_) {
                flushRequested_ = false;
                notifyFlush = true;   // 标志需要 flush
            }
        }

        // 删除超量的buffer
        if (bufferToWrite.size() > static_cast<std::size_t>(maxQueueBuffers_)) {
            const std::size_t dropped = bufferToWrite.size() - maxQueueBuffers_;
            droppedLogs_.fetch_add(dropped, std::memory_order_relaxed);
            bufferToWrite.erase(bufferToWrite.begin() + maxQueueBuffers_, bufferToWrite.end());
        }

        // 锁外写入文件和 stdout
        for (const auto& buffer : bufferToWrite) {
            if (toStdout_) {
                std::fwrite(buffer->data(), 1, buffer->length(), stdout);
            }

            if (output) {
                output->append(buffer->data(), buffer->length());
            }
        }

        // 写完后刷新
        if (toStdout_) {
            std::fflush(stdout);
        }

        if (output) {
            output->flush();
        }

        if (notifyFlush) {
            std::lock_guard<std::mutex> lock(mutex_);
            flushDone_ = true;         // 标志 强制 flush 完成
            flushCond_.notify_all();   // 唤醒 flush()
        }

        // 调整 vector 大小，避免 vector 长时间持有大内存
        if (bufferToWrite.size() > 2) {
            bufferToWrite.resize(2);
        }

        // 补充 newBuffer1
        if (!newBuffer1 && !bufferToWrite.empty()) {
            newBuffer1 = std::move(bufferToWrite.back());
            bufferToWrite.pop_back();
            newBuffer1->clear();
        }

        // 补充 newBuffer2
        if (!newBuffer2 && !bufferToWrite.empty()) {
            newBuffer2 = std::move(bufferToWrite.back());
            bufferToWrite.pop_back();
            newBuffer2->clear();
        }

        bufferToWrite.clear();
    }

    // 线程退出前刷新
    if (toStdout_) {
        std::fflush(stdout);
    }

    if (output) {
        output->flush();
    }
}