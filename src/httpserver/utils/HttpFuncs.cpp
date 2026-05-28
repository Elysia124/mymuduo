#include "httpserver/utils/HttpFuncs.h"

#include <algorithm>
#include <ranges>
int hexToInt(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

// 解析 url 中的 '&' '+' 和 ‘ ’
std::string urlDecode(std::string_view url, bool plusAsSpace)
{
    std::string result;
    result.reserve(url.size());

    for (std::size_t i = 0; i < url.size(); ++i) {
        char c = url[i];

        if (c == '%' && i + 2 < url.size()) {
            int high = hexToInt(url[i + 1]);
            int low = hexToInt(url[i + 2]);

            if (high != -1 && low != -1) {
                result.push_back(static_cast<char>((high << 4) | low));
                i += 2;
            }
            else {
                result.push_back(c);
            }
        }
        else if (c == '+' && plusAsSpace) {
            result.push_back(' ');
        }
        else {
            result.push_back(c);
        }
    }
    return result;
}

void trim(std::string& str)
{
    auto is_not_space = [](unsigned char c) { return !std::isspace(c); };
    // 去除 str 左边的空格
    str.erase(str.begin(), std::ranges::find_if(str, is_not_space));

    // 删除右边的空格
    str.erase(std::ranges::find_if(str | std::views::reverse, is_not_space).base(), str.end());
}

void stringToLower(std::string& str)
{
    std::ranges::transform(
        str, str.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
}
