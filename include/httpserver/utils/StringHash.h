#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace mymuduo::http {

// 透明 String Hash
struct TransparentStringHash
{
    using is_transparent = void;

    std::size_t operator()(std::string_view value) const noexcept
    {
        return std::hash<std::string_view>{}(value);
    }

    std::size_t operator()(const std::string& value) const noexcept
    {
        return std::hash<std::string_view>{}(value);
    }

    std::size_t operator()(const char* value) const noexcept { return std::hash<std::string_view>{}(value); }
};

// 大小写不敏感的透明 String Hash
struct CaseInsensitiveStringHash
{
    using is_transparent = void;

    std::size_t operator()(std::string_view value) const noexcept
    {
        std::uint64_t hash = 14695981039346656037ULL;
        for (char ch : value) {
            hash ^= static_cast<unsigned char>(asciiToLower(ch));
            hash *= 1099511628211ULL;
        }
        return static_cast<std::size_t>(hash);
    }

    std::size_t operator()(const std::string& value) const noexcept
    {
        return (*this)(std::string_view(value));
    }

    std::size_t operator()(const char* value) const noexcept { return (*this)(std::string_view(value)); }

private:
    static char asciiToLower(char ch) noexcept
    {
        return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : ch;
    }
};

// 大小写不敏感的 String Comparator
struct CaseInsensitiveStringEqual
{
    using is_transparent = void;

    bool operator()(std::string_view lhs, std::string_view rhs) const noexcept
    {
        if (lhs.size() != rhs.size()) {
            return false;
        }

        for (std::size_t i = 0; i < lhs.size(); ++i) {
            if (asciiToLower(lhs[i]) != asciiToLower(rhs[i])) {
                return false;
            }
        }
        return true;
    }

private:
    static char asciiToLower(char ch) noexcept
    {
        return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : ch;
    }
};
}   // namespace mymuduo::http
