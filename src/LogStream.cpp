#include "LogStream.h"
#include <cstdint>
#include <cstring>

using namespace mymuduo;

LogStream& LogStream::operator<<(bool value)
{
    buffer_.append(value ? "1" : "0", 1);
    return *this;
}

LogStream& LogStream::operator<<(char value)
{
    buffer_.append(&value, 1);
    return *this;
}

LogStream& LogStream::operator<<(const char* str)
{
    if (str) {
        return *this << std::string_view(str);
    }

    buffer_.append("(null)", 6);
    return *this;
}


LogStream& LogStream::operator<<(std::string_view str)
{
    buffer_.append(str.data(), str.size());
    return *this;
}

LogStream& LogStream::operator<<(const void* ptr)
{
    constexpr char HexDigits[] = "0123456789abcdef";
    constexpr int PointStringLen = 18;

    if (buffer_.avail() >= PointStringLen) {
        char* buf = buffer_.current();

        buf[0] = '0';
        buf[1] = 'x';

        auto value = reinterpret_cast<std::uintptr_t>(ptr);

        int index = 2;

        for (int i = 60; i >= 0; i -= 4) { buf[index++] = HexDigits[(value >> i) & 0x0f]; }

        buffer_.add(PointStringLen);
    }
    return *this;
}
