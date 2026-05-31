#pragma once

#include <string>
#include <string_view>
#include <cctype>

int hexToInt(char c);


// 解析 url 中的 '&' '+' 和 ‘ ’
std::string urlDecode(std::string_view url, bool plusAsSpace = true);

void trim(std::string& str);

void stringToLower(std::string& str);

