#pragma once

#include "noncopyable.h"
#include <charconv>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <system_error>
#include <type_traits>
namespace mymuduo {
namespace detail {
constexpr int kSmallBuffer = 4000;
template<int SIZE>
class FixedBuffer : noncopyable
{
public:
    FixedBuffer() : cur_(data_) {}

    void append(const char* buf, size_t len)
    {
        if (static_cast<size_t>(avail()) >= len) {
            std::memcpy(cur_, buf, len);
            cur_ += len;
        }
    }

    const char* data() const { return data_; }

    int length() const { return static_cast<int>(cur_ - data_); }

    int avail() const { return static_cast<int>(end() - cur_); }

    char* current() { return cur_; }

    void add(size_t len) { cur_ += len; }

    void reset() { cur_ = data_; }

private:
    const char* end() const { return data_ + sizeof(data_); }
    char data_[SIZE];
    char* cur_;
};
}   // namespace detail

// T是整数或浮点数、但不是 bool 和 char
template<typename T>
concept Numeric =
    (std::is_integral_v<T> || std::is_floating_point_v<T>) && !std::is_same_v<T, bool> && !std::is_same_v<T, char>;

class LogStream : noncopyable
{
public:
    using Buffer = detail::FixedBuffer<detail::kSmallBuffer>;
    LogStream& operator<<(bool value);

    // c++17
    //  template<typename Integer,
    //           typename = std::enable_if_t<std::is_integral_v<Integer> && !std::is_same_v<Integer, bool>>>
    //  LogStream& operator<<(Integer value);

    // c++20
    template<Numeric T>
    LogStream& operator<<(T value)
    {
        formatNumeric(value);
        return *this;
    }

    LogStream& operator<<(char value);

    LogStream& operator<<(const char* str);

    LogStream& operator<<(std::string_view str);

    template<size_t N>
    LogStream& operator<<(const char (&str)[N])
    {
        buffer_.append(str, N - 1);
        return *this;
    }

    LogStream& operator<<(const void* ptr);

    const Buffer& buffer() const { return buffer_; }

private:
    template<typename T>
    void formatNumeric(T value)
    {
        if (buffer_.avail() >= kMaxNumericSize) {

            if constexpr (std::is_integral_v<T>) {
                if (auto [ptr, ec] = std::to_chars(buffer_.current(), buffer_.current() + kMaxNumericSize, value);
                    ec == std::errc()) {

                    size_t len = ptr - buffer_.current();
                    buffer_.add(len);
                }
            }
            else {
                if (auto [ptr, ec] = std::to_chars(
                        buffer_.current(), buffer_.current() + kMaxNumericSize, value, std::chars_format::general);
                    ec == std::errc()) {

                    size_t len = ptr - buffer_.current();
                    buffer_.add(len);
                }
            }
        }
    }

    static constexpr int kMaxNumericSize = 48;   // 数字转为字符串后的字符串最大长度(带冗余)
    Buffer buffer_;
};

}   // namespace mymuduo
