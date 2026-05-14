#include "Thread.h"
#include "CurrentThread.h"
#include <cstdio>
#include <semaphore.h>
#include <thread>


using namespace mymuduo;

Thread::Thread(ThreadFunc func, const std::string& n)
    : started_(false), joined_(false), tid_(0), func_(std::move(func)), name_(n)
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_) { thread_.detach(); }
}

void Thread::start()
{
    started_ = true;

    sem_t sem;   // 信号量
    sem_init(&sem, 0, 0);

    thread_ = std::thread([&]() {
        tid_ = CurrentThread::tid();
        sem_post(&sem);   // +1
        func_();
    });

    // 确保能够获取到线程的 tid_
    sem_wait(&sem);
}

void Thread::join()
{
    joined_ = true;
    thread_.join();
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