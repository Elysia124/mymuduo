#include "Buffer.h"
#include <bits/types/struct_iovec.h>
#include <cerrno>
#include <cstddef>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

using namespace mymuduo;

/*
the purpose of temporary buffer is to read as much data as possible in a single operation
*/

ssize_t Buffer::readFd(int fd, int& saveErrno)
{
    char extrabuf[65536];   // temporary buffer in stack(64k)

    iovec vec[2];
    const size_t writable = writableBytes();

    // vec[0] -> buffer in heap(vector)
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    // vec[1] -> temporary buffer
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    // buffer 自己空间已经够大(>= 64k)时，就直接读到 buffer；如果还读不完，下次再读。
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;

    // readv: 从 fd 读取数据并同时写入多个缓冲区
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        saveErrno = errno;
    }
    else if (static_cast<size_t>(n) <= writable) {
        writerIndex_ += n;
    }
    else {   // there are data in extrabuf
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }

    return n;
}

ssize_t Buffer ::writeFd(int fd, int& saveErrno) const
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0) {
        saveErrno = errno;
    }
    return n;
}