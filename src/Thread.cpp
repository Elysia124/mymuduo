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

    // 初始化捕获：
    // 1. tid = &tid_：保存 Thread::tid_ 的地址，子线程启动后写入真实 tid
    // 2. &sem：引用捕获局部信号量，用于通知 start()：tid_ 已经设置好了
    // 3. func = std::move(func_)：把成员 func_ 移动到线程函数自己的闭包对象里
    // 这样子线程执行用户回调时，调用的是 lambda 自己持有的 func，
    // 而不是 this->func_，可以减少对子线程期间 Thread 对象本身的依赖。
    thread_ = std::thread([tid = &tid_, &sem, func = std::move(func_)]() mutable {
        *tid = CurrentThread::tid();

        // 通知 start()：tid_ 已经写好了，可以继续往下走
        sem_post(&sem);   // +1

        // 执行用户传入的线程函数
        func();
    });

    // 确保能够获取到线程的 tid_
    // sem_wait 可能被信号中断，errno == EINTR 时应该继续等待
    while (sem_wait(&sem) != 0) {
        if (errno != EINTR) {
            LOG_FATAL("sem_wait error\n");
        }
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
