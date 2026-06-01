#include "net/FileUtil.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <string_view>
#include <utility>

namespace {

// 自动创建日志目录
// 如 logs/xxx.log，如果目录 logs 不存在则创建
void ensureParentPath(std::string_view filename)
{
    try {
        std::filesystem::path path(filename);
        auto parent = path.parent_path();
        if (!parent.empty()) {   // 存在父目录(如logs/)
            std::filesystem::create_directories(parent);
        }
    }
    catch (...) {
    }
}
}   // namespace

using namespace mymuduo;

AppendFile::AppendFile(std::string filename, bool append) : filename_(std::move(filename))
{
    ensureParentPath(filename_);
    fp_ = std::fopen(filename_.c_str(), append ? "ae" : "we");
    if (!fp_) {
        std::cerr << "AppendFile open failed, file=" << filename_ << ", errno=" << errno
                  << ", error=" << std::strerror(errno) << '\n';
        return;
    }

    std::setvbuf(fp_, buffer_, _IOFBF, sizeof(buffer_));
}

AppendFile::~AppendFile()
{
    if (fp_) {
        flush();
        std::fclose(fp_);
        fp_ = nullptr;
    }
}

void AppendFile::append(const char* logline, std::size_t len)
{
    if (!logline || !fp_ || len == 0) {
        return;
    }

    std::size_t written = 0;
    while (written < len) {
        std::size_t n = write(logline + written, len - written);
        if (n == 0) {
            std::cerr << "AppendFile write failed, file=" << filename_ << ", errno=" << errno
                      << ", error=" << std::strerror(errno) << '\n';
        }

        written += n;
    }

    writeBytes_ = static_cast<off_t>(written);
}

void AppendFile::flush()
{
    if (fp_) {
        std::fflush(fp_);
    }
}

std::size_t AppendFile::write(const char* logline, std::size_t len)
{
    return std::fwrite(logline, 1, len, fp_);
}