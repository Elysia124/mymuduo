#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <vector>
namespace mymuduo {
class Buffer
{
public:
    static constexpr std::size_t kCheapPrepend = 8;     // 预留空间，方便添加协议头、长度字段等
    static constexpr std::size_t kInitialSize = 1024;   // 缓冲区默认大小

    explicit Buffer(std::size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize), readerIndex_(kCheapPrepend), writerIndex_(kCheapPrepend)
    {}

    // 返回可读字节数
    std::size_t readableBytes() const { return writerIndex_ - readerIndex_; }

    // 返回可写字节数
    std::size_t writableBytes() const { return buffer_.size() - writerIndex_; }

    // 返回(预留 + 可回收空间)字节数
    std::size_t prependableBytes() const { return readerIndex_; }

    // 返回缓冲区可读数据的起始地址
    const char* peek() const { return begin() + readerIndex_; }

    // 调整读指针
    void retrieve(std::size_t len)
    {
        if (len < readableBytes()) {
            readerIndex_ += len;
        }
        else {
            retrieveAll();
        }
    }

    void retrieveAll() { readerIndex_ = writerIndex_ = kCheapPrepend; }

    std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); };

    std::string retrieveAsString(std::size_t len)
    {
        len = std::min(len, readableBytes());
        std::string result(peek(), len);
        retrieve(len);
        return result;
    };

    void retrieveUntil(const char* end)
    {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }

    void ensureWritableBytes(std::size_t len)
    {
        if (writableBytes() < len) {
            // 可读字节数 < 需要写入的字节数
            makeSpace(len);
        }
    }

    // 把 [data,data+len]上的数据拷贝进缓冲区
    void append(const char* data, std::size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    void append(std::string_view str) { append(str.data(), str.size()); }

    const char* findCRLF() const { return findCRLF(peek()); }

    const char* findCRLF(const char* start) const
    {
        assert(start < beginWrite());
        assert(peek() <= start);
        std::string_view sv(start, static_cast<std::size_t>(beginWrite() - start));
        std::size_t pos = sv.find("\r\n");

        return pos == std::string_view::npos ? nullptr : start + pos;
    }

    char* beginWrite() { return begin() + writerIndex_; }
    const char* beginWrite() const { return begin() + writerIndex_; }

    // 从 fd 上读数据
    ssize_t readFd(int fd, int& saveErrno);

    ssize_t writeFd(int fd, int& saveErrno) const;

private:
    // 返回 vector 底层数组的首地址
    char* begin() { return buffer_.data(); }

    const char* begin() const { return buffer_.data(); }

    void makeSpace(std::size_t len)
    {
        // 判断能否通过移动已有数据完成
        // 可回收的空间 prependableBytes() - kCheapPrepend
        //  writableBytes() + (prependableBytes() - kCheapPrepend) >= len
        if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
            // 不能通过移动数据完成，必须扩容
            buffer_.resize(len + writerIndex_);
        }
        else {
            // 可通过移动数据完成
            std::size_t readable = readableBytes();   // 可读字节数

            // 从后往前移动数据，可以用copy，数据重叠部分不会有问题
            // 如果需要从前往后移动数据，用std::copy_backward(src_begin, src_end, dst_end)
            // 它是从 src_end - 1 开始搬到 dst_end - 1 位置，然后一路往前拷贝
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);

            readerIndex_ = kCheapPrepend;             // 新的可读起点
            writerIndex_ = readerIndex_ + readable;   // 新的可写起点
        }
    }

    std::vector<char> buffer_;
    std::size_t readerIndex_;   // 可读字节起始地址
    std::size_t writerIndex_;   // 可写字节起始地址
};
}   // namespace mymuduo
