#include "Thread.h"
#include "CurrentThread.h"
#include "Logger.h"
#include <cassert>
#include <cstdio>
#include <semaphore.h>
#include <thread>


using namespace mymuduo;

Thread::Thread(ThreadFunc func, std::string n)
    : started_(false), joined_(false), tid_(0), func_(std::move(func)), name_(std::move(n))
{
    setDefaultName();
}

Thread::~Thread()
{
    if (thread_.joinable()) {
        thread_.detach();
    }
}

void Thread::start()
{
    assert(!started_);
    started_ = true;

    sem_t sem;   // 信号量
    if (sem_init(&sem, 0, 0) != 0) {
        LOG_FATAL("sem_init error\n");
    }

    thread_ = std::thread([&]() {
        tid_ = CurrentThread::tid();
        sem_post(&sem);   // +1
        func_();
    });

    // 确保能够获取到线程的 tid_
    if (sem_wait(&sem) != 0) {
        LOG_FATAL("sem_wait error\n");
    }

    sem_destroy(&sem);
}

void Thread::join()
{
    if (thread_.joinable()) {
        joined_ = true;
        thread_.join();
    }
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty()) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Thread %d", num);
        name_ = buf;
    }
}
