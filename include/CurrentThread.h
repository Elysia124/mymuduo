#pragma once
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>


namespace mymuduo::CurrentThread {
extern thread_local int t_cachedTid;
void cacheTid();

inline int tid()
{
    if (__builtin_expect(t_cachedTid == 0, 0)) { cacheTid(); }

    return t_cachedTid;
}
}   // namespace mymuduo::CurrentThread