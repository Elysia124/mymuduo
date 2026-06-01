#pragma once

#include "net/noncopyable.h"
#include <cstddef>
#include <cstdio>
#include <string>
namespace mymuduo {

// 负责写入文件
class AppendFile : noncopyable
{
public:
    explicit AppendFile(std::string filename, bool append = true);
    ~AppendFile();

    void append(const char* logline, std::size_t len);

    void flush();

    off_t writeBytes() const noexcept { return writeBytes_; }

    bool valid() const { return fp_ != nullptr; }

    const std::string& filename() const noexcept { return filename_; }

private:
    std::size_t write(const char* logline, std::size_t len);

    std::string filename_;
    std::FILE* fp_;
    char buffer_[64 * 1024];
    off_t writeBytes_ = 0;
};
}   // namespace mymuduo