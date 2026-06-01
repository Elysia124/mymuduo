#include "net/LogFile.h"
#include "net/FileUtil.h"
#include <cassert>
#include <ctime>
#include <memory>
#include <mutex>
#include <unistd.h>
#include <utility>

using namespace mymuduo;

namespace {
std::string hostname()
{
    char buf[256];
    if (::gethostname(buf, sizeof(buf)) == 0) {
        buf[sizeof(buf) - 1] = '\0';
        return buf;
    }

    return "unknownhost";
}
}   // namespace

LogFile::LogFile(std::string basename, off_t rollSize, bool threadSafe, int flushInterval, int checkEceryN,
                 bool append)
    : baseneme_(std::move(basename))
    , rollSize_(rollSize)
    , flushInterval_(flushInterval > 0 ? flushInterval : 4)
    , checkEveryN_(checkEceryN > 0 ? checkEceryN : 1024)
    , append_(append)
    , mutex_(threadSafe ? std::make_unique<std::mutex>() : nullptr)
{
    assert(rollSize_ > 0);

    rollFile();
}

LogFile::~LogFile() = default;

void LogFile::append(const char* logline, std::size_t len)
{
    if (!logline || len == 0) {
        return;
    }

    if (mutex_) {
        std::lock_guard<std::mutex> lock(*mutex_);
        appendUnlock(logline, len);
    }
    else {
        appendUnlock(logline, len);
    }
}

void LogFile::flush()
{
    if (!file_) {
        return;
    }

    if (mutex_) {
        std::lock_guard<std::mutex> lock(*mutex_);
        file_->flush();
    }
    else {
        file_->flush();
    }
}

// 强制滚动文件
bool LogFile::rollFile()
{
    std::time_t now;
    const std::string filename = getLogFileName(baseneme_, &now);
    const std::time_t start = now / kRollPerSeconds * kRollPerSeconds;

    if (now < lastRoll_) {
        return false;
    }

    lastFlush_ = now;
    lastRoll_ = now;
    startOfPeriod_ = start;

    file_ = std::make_unique<AppendFile>(filename, append_);
    return file_ && file_->valid();
}

void LogFile::appendUnlock(const char* logline, std::size_t len)
{
    if (!file_ || len == 0) {
        return;
    }

    file_->append(logline, len);

    if (file_->writeBytes() > rollSize_) {
        rollFile();
        return;
    }

    ++count_;
    if (count_ == checkEveryN_) {
        const std::time_t now = ::time(nullptr);
        const std::time_t thisPeriod = now / kRollPerSeconds * kRollPerSeconds;   // 向下取整，得到周期

        if (thisPeriod != startOfPeriod_) {
            rollFile();
        }
        else if (now - lastFlush_ > flushInterval_) {
            flush();
            lastFlush_ = now;
        }
    }
}

std::string LogFile::getLogFileName(const std::string& basename, std::time_t* now)
{
    std::string filename;
    filename.reserve(basename.size() + 64);

    filename += basename;

    char timebuf[64];
    *now = ::time(nullptr);

    std::tm tm;
    ::localtime_r(now, &tm);
    std::snprintf(timebuf,
                  sizeof timebuf,
                  ".%04d%02d%02d-%02d%02d%02d",
                  tm.tm_year + 1900,
                  tm.tm_mon + 1,
                  tm.tm_mday,
                  tm.tm_hour,
                  tm.tm_min,
                  tm.tm_sec);

    // 文件名带 hostname。
    filename += timebuf;
    filename += hostname();

    // 文件名带 pid
    char pidbuf[32];
    std::snprintf(pidbuf, sizeof pidbuf, ".%d", static_cast<int>(::getpid()));
    filename += pidbuf;
    filename += ".log";

    return filename;
}
