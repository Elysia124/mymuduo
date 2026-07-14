# MyMuduo 项目知识点与面试问答手册

> 适用范围：C++ 后端、Linux 网络编程、基础架构、服务端研发岗位  
> 项目技术栈：C++17、Linux、epoll、Reactor、POSIX Thread、Socket、CMake、HTTP/1.1、MySQL Connector/C++  
> 文档依据：当前仓库实际实现。没有压测依据的数字不要在简历或面试中虚构。

---

## 目录

1. [项目全景与简历表述边界](#一项目全景与简历表述边界)
2. [网络编程知识点与面试问答](#二网络编程知识点与面试问答)
3. [C++ 知识点与面试问答](#三c-知识点与面试问答)
4. [并发与操作系统知识点与面试问答](#四并发与操作系统知识点与面试问答)
5. [HTTP、路由、中间件与 Session](#五http路由中间件与-session)
6. [日志、数据库与工程化](#六日志数据库与工程化)
7. [项目设计类综合问题](#七项目设计类综合问题)
8. [缺陷、边界与改进方向](#八缺陷边界与改进方向)
9. [面试自述模板](#九面试自述模板)
10. [面试前速查清单](#十面试前速查清单)

---

# 一、项目全景与简历表述边界

## 1.1 项目定位

MyMuduo 是一个运行于 Linux 的 C++17 网络库，并在 TCP 网络层之上扩展了 HTTP 服务能力。它不是单纯的 Echo Server，而是包含以下层次：

```text
应用层
  ├─ HTTP Handler
  ├─ Router：静态路由、:param、* 通配符
  ├─ Middleware / CORS
  ├─ Cookie / Session
  └─ 可选 MySQL 连接池
          │
HTTP 层   ├─ HttpServer
          ├─ HttpContext 增量状态机
          ├─ HttpRequest
          └─ HttpResponse
          │
TCP 层    ├─ TcpServer / TcpClient
          ├─ TcpConnection
          ├─ Acceptor / Connector
          └─ Buffer
          │
Reactor   ├─ EventLoop
          ├─ Channel
          ├─ Poller / EPollPoller
          ├─ EventLoopThreadPool
          └─ TimerQueue
          │
Linux     ├─ epoll
          ├─ eventfd
          ├─ timerfd
          ├─ non-blocking socket / accept4 / readv
          └─ pthread / mutex / condition_variable
```

## 1.2 一次 TCP 连接的完整流转

1. `Acceptor` 创建非阻塞监听 Socket，并把监听 fd 封装成 `Channel` 注册到 base loop。
2. `epoll_wait` 返回监听 fd 的可读事件，`Acceptor::handleRead` 循环执行 `accept4`，直到 `EAGAIN`。
3. `TcpServer` 从 `EventLoopThreadPool` 轮询选择一个 sub loop。
4. `TcpServer` 创建 `TcpConnection`，将连接保存到连接表，并把建立连接的任务投递到 sub loop。
5. `TcpConnection::connectEstablished` 通过 `Channel::tie` 绑定生命周期，然后开启读事件。
6. 连接有数据到达时，sub loop 分发读回调，`Buffer::readFd` 使用 `readv` 读取数据。
7. 上层消息回调处理数据并调用 `send`。如果一次没有写完，剩余数据进入输出缓冲区，同时监听 `EPOLLOUT`。
8. 输出缓冲区清空后取消 `EPOLLOUT`，必要时触发写完成回调或执行半关闭。
9. 对端关闭或本端强制关闭时，连接先从 sub loop 的 Poller 移除，再通知 base loop 从连接表删除。

## 1.3 一次 HTTP 请求的完整流转

1. TCP 消息回调进入 `HttpServer::onMessage`。
2. 每条连接通过 `std::any` 保存独立的 `HttpConnectionContext`。
3. `HttpContext` 按“请求行—请求头—请求体—完成”四个状态增量解析数据。
4. 同一个 Buffer 中可能包含多个请求，因此 `onMessage` 循环解析，实现 HTTP Pipeline 的顺序处理。
5. 请求先按注册顺序执行中间件 `before`。
6. Router 根据 HTTP Method 和路径匹配 Handler，匹配优先级是静态节点、动态参数节点、通配符节点。
7. 响应阶段按逆序执行中间件 `after`。
8. `HttpResponse` 自动生成 `Connection`、`Content-Length` 等报文信息并发送。
9. 根据 HTTP 版本和 `Connection` 请求头决定保持连接或半关闭连接。

## 1.4 简历可以写什么

- 基于 Linux `epoll` 实现 Reactor 网络库。
- 使用主从 Reactor 与 one loop per thread 模型处理多连接。
- 使用 `eventfd` 实现跨线程唤醒，使用 `timerfd` 实现定时任务。
- 实现非阻塞 TCP Server/Client、连接重试、输入输出 Buffer、高水位回调。
- 实现 HTTP/1.0、HTTP/1.1 增量解析、Keep-Alive、Pipeline 顺序处理、路由、中间件、CORS、Cookie、Session。
- 实现同步/异步日志以及可选 MySQL 连接池。
- 编写 15 项测试，并配置 ASan、UBSan、TSan、Valgrind 检查流程。

## 1.5 简历暂时不要写什么

- 不要写“支持百万并发”“QPS 达到 XX 万”，因为仓库没有可复现的基准测试报告。
- 不要写“零拷贝”，当前发送路径仍会经过用户态 Buffer，也没有实现 `sendfile`、`splice`。
- 不要写“无锁日志”，异步日志前端使用了互斥锁，只是将磁盘 I/O 移到了后台线程。
- 不要写“完整支持 HTTP/1.1”，当前不支持 `Transfer-Encoding: chunked`，也没有 TLS、WebSocket、HTTP/2。
- 不要写“线程池处理业务任务”。当前 `EventLoopThreadPool` 是 I/O 线程池，耗时业务仍需要单独的计算线程池。

---

# 二、网络编程知识点与面试问答

## 2.1 Reactor 模式

### 知识点

Reactor 的核心是让一个事件分发器等待多个 fd 的就绪事件，再将事件交给预先注册的回调处理。当前项目中的对应关系是：

| Reactor 概念 | 项目类 |
|---|---|
| 事件循环 | `EventLoop` |
| I/O 多路复用器 | `Poller` / `EPollPoller` |
| 事件源及回调 | `Channel` |
| TCP 连接 | `TcpConnection` |
| 新连接接收器 | `Acceptor` |
| 跨线程任务队列 | `pendingFunctors_` |

### 问题 1：什么是 Reactor？项目中是怎么实现的？

**参考回答：**

Reactor 是一种事件驱动网络模型。线程不是阻塞在某一个连接的读写上，而是阻塞在 `epoll_wait`，一次等待多个 fd 的事件。项目中 `EventLoop` 负责循环，`EPollPoller` 负责等待事件，`Channel` 封装 fd、感兴趣事件和回调。`epoll_wait` 返回后，Poller 将活跃 Channel 交给 EventLoop，EventLoop 再调用 Channel 的读、写、关闭和错误回调。

### 问题 2：Reactor 和 Proactor 有什么区别？

**参考回答：**

Reactor 通知的是“fd 已经可以读写”，真正的 `read/write` 仍由应用执行；Proactor 通知的是“异步 I/O 已经完成”，应用直接处理结果。本项目基于 Linux `epoll`，属于同步非阻塞 I/O 加 Reactor，而不是 Proactor。

### 问题 3：为什么不采用一个连接一个线程？

**参考回答：**

一个连接一个线程在连接数上升后会带来较大的线程栈内存、上下文切换和调度开销，而且大量连接大部分时间是空闲的。Reactor 用少量 I/O 线程管理大量连接，只有 fd 就绪时才处理对应事件，更适合 I/O 密集型服务。

## 2.2 主从 Reactor 与 one loop per thread

### 问题 4：你的主从 Reactor 是怎样分工的？

**参考回答：**

base loop 负责监听 Socket 和接收新连接；收到连接后，`TcpServer` 从 `EventLoopThreadPool` 中轮询选择一个 sub loop。连接建立、读写和关闭主要由该 sub loop 负责。这样接收连接与连接 I/O 被分离，同时每条连接始终绑定一个 EventLoop，避免多个线程同时操作同一个 Channel。

### 问题 5：one loop per thread 有什么好处？

**参考回答：**

每个 EventLoop 只属于一个线程，Channel 和 Poller 的修改都在所属线程完成。它把共享状态转化为线程内状态，减少了 I/O 路径中的锁竞争。跨线程操作不直接修改连接，而是投递到目标 loop 的任务队列。

### 问题 6：线程数越多性能越好吗？

**参考回答：**

不是。I/O 线程太多会增加上下文切换、缓存失效和调度开销。通常可以从 CPU 核数、连接负载和回调耗时出发调节。如果业务回调包含耗时计算或阻塞数据库操作，应该增加独立业务线程池，而不是盲目增加 Reactor 线程数。

## 2.3 epoll

### 问题 7：select、poll 和 epoll 有什么区别？

**参考回答：**

- `select` 使用位图表示 fd 集合，存在 fd 数量限制，并且每次调用都要复制和遍历整个集合。
- `poll` 使用数组，避免固定的位图上限，但仍要每次线性扫描全部 fd。
- `epoll` 在内核维护关注集合，通过 `epoll_ctl` 增删改 fd，`epoll_wait` 返回就绪事件，活跃连接较少时更高效。

不能简单说 epoll 永远是 O(1)。`epoll_ctl`、内核就绪队列、回调处理都存在成本；优势主要是避免每次扫描全部关注 fd。

### 问题 8：项目使用 LT 还是 ET？如何判断？

**参考回答：**

项目使用 LT，因为注册事件时没有设置 `EPOLLET`。LT 下只要 fd 仍然可读或可写，epoll 会继续通知；ET 只在状态变化时通知，必须一直读写到 `EAGAIN`。当前 Acceptor 会循环 `accept4` 到 `EAGAIN`，但普通连接的读路径一次执行一次 `readv`，这与 LT 是匹配的。

### 问题 9：LT 和 ET 各有什么优缺点？

**参考回答：**

LT 编程简单、容错更好，即使一次没读完，下次仍会通知，但可能产生更多事件通知。ET 通知次数少，适合高性能场景，但必须使用非阻塞 fd，并在每次事件中处理到 `EAGAIN`，否则可能丢失后续处理机会。

### 问题 10：为什么 EPOLLOUT 不能一直监听？

**参考回答：**

Socket 发送缓冲区通常大部分时间可写。如果一直监听 `EPOLLOUT`，EventLoop 会被无意义地持续唤醒，形成 busy loop。项目只在用户数据未一次写完、输出缓冲区存在积压时开启写事件；缓冲区清空后立即关闭写事件。

### 问题 11：`epoll_event.data.ptr` 为什么存 Channel 指针？

**参考回答：**

这样 `epoll_wait` 返回后可以直接获得对应 Channel，避免再次通过 fd 查表才能找到回调对象。前提是 Channel 在注册期间必须存活，因此项目还需要严格管理 Channel 与 TcpConnection 的生命周期。

## 2.4 非阻塞 Socket

### 问题 12：为什么网络 fd 要设置为非阻塞？

**参考回答：**

如果 fd 是阻塞的，即使 epoll 报告就绪，单次读写也可能因为竞态或数据量变化而阻塞整个 EventLoop，导致同一线程上的其他连接无法处理。项目在创建监听 Socket、连接 Socket 和 `accept4` 时直接设置 `SOCK_NONBLOCK | SOCK_CLOEXEC`。

### 问题 13：非阻塞 connect 为什么要监听可写事件？

**参考回答：**

非阻塞 `connect` 常返回 `EINPROGRESS`，表示连接正在建立。当 fd 可写时，不能直接认定连接成功，而要读取 `SO_ERROR`：为 0 才表示成功，否则是具体错误。项目 `Connector::handleWrite` 就是这样判断，并额外检查自连接。

### 问题 14：项目如何实现客户端重试？

**参考回答：**

连接失败后关闭旧 fd，通过 EventLoop 的定时器延迟重新发起连接。延迟采用指数退避：初始 500ms，每次翻倍，上限 30s。这样可避免服务端故障期间客户端频繁重连造成惊群或持续占用 CPU。

### 问题 15：`SO_REUSEADDR` 和 `SO_REUSEPORT` 有什么区别？

**参考回答：**

`SO_REUSEADDR` 常用于服务重启时重新绑定仍受 TIME_WAIT 影响的地址，也影响通配地址绑定规则；`SO_REUSEPORT` 允许多个 Socket 绑定同一地址端口，内核可在它们之间分发连接。本项目默认开启 `SO_REUSEADDR`，`SO_REUSEPORT` 由 TcpServer 选项控制。

### 问题 16：`TCP_NODELAY` 与 `SO_KEEPALIVE` 分别解决什么问题？

**参考回答：**

`TCP_NODELAY` 关闭 Nagle 算法，减少小包等待，适合低延迟场景；`SO_KEEPALIVE` 使用 TCP 保活探测发现长期失效连接，但默认探测周期通常很长。项目为连接开启了 `SO_KEEPALIVE`；HTTP 层另外用 60 秒应用层空闲定时器管理 Keep-Alive，二者用途不同。

## 2.5 eventfd 与跨线程唤醒

### 问题 17：EventLoop 阻塞在 epoll_wait，其他线程怎样让它执行任务？

**参考回答：**

其他线程先把 Functor 放入由互斥锁保护的 `pendingFunctors_`，再向 `eventfd` 写入一个 64 位整数。EventLoop 将该 eventfd 注册到 epoll，所以写入后 epoll_wait 立即返回，读回调消费计数，然后 `doPendingFunctors` 交换并执行任务队列。

### 问题 18：为什么使用 eventfd，而不是条件变量？

**参考回答：**

条件变量不能直接被 epoll 监听；eventfd 本身是 fd，可以统一纳入 Reactor。也可以使用 pipe 或 socketpair，但 eventfd 更轻量，语义就是事件计数，读写固定的 8 字节整数。

### 问题 19：`queueInLoop` 为什么把队列 swap 到局部变量后再执行？

**参考回答：**

这样锁只保护队列交换，不在执行用户回调时持锁。它能降低生产者等待时间，也避免回调内部再次投递任务导致死锁。新投递的任务会进入下一轮队列。

### 问题 20：为什么在本线程且正在执行 pending functors 时也要 wakeup？

**参考回答：**

当前 `doPendingFunctors` 已经把成员队列交换到局部变量。如果正在执行的回调又投递新任务，新任务不会出现在当前局部列表中。唤醒可避免下一轮 `epoll_wait` 最长阻塞到默认超时，保证新任务尽快执行。

## 2.6 Buffer、粘包与部分写

### 问题 21：TCP 为什么会出现粘包、拆包？

**参考回答：**

TCP 是字节流，没有应用消息边界。发送方的多次 send 可能合并，单次 send 也可能被拆分，所以接收方必须依据应用协议划分消息。常见做法有固定长度、分隔符、长度字段和自描述协议。HTTP 使用请求行、CRLF、头部和 Content-Length 定义边界。

### 问题 22：项目的 Buffer 是怎样设计的？

**参考回答：**

Buffer 基于连续 `vector<char>`，维护 `readerIndex` 和 `writerIndex`，逻辑上分为 prependable、readable、writable 三段。前部预留 8 字节，方便协议编码时前插长度等字段。空间不足时优先搬移可读数据回收前部空间，不够才扩容。

### 问题 23：为什么使用 readv？

**参考回答：**

项目准备两个 iovec：第一个指向 Buffer 当前可写区，第二个指向栈上的 64KB 临时缓冲区。一次 `readv` 可以尽量读完内核接收缓冲区的数据。如果数据超过 Buffer 可写空间，额外部分先落到临时区，再 append 到 Buffer。这样避免每次读取前都把用户 Buffer 扩到很大。

### 问题 24：send 返回值为什么可能小于请求长度？

**参考回答：**

非阻塞 Socket 的内核发送缓冲区可能没有足够空间，因此 `write` 只接受部分数据，甚至返回 `EAGAIN`。项目先尝试直接写；未写完的部分追加到 `outputBuffer_`，并开启 `EPOLLOUT`，之后在可写回调中继续发送。

### 问题 25：什么是高水位回调？它等于完整的背压吗？

**参考回答：**

当输出 Buffer 从阈值以下增长到阈值以上时，项目触发高水位回调，用于通知上层生产速度已经超过发送速度。上层可以暂停读取、限流或停止生成数据。但当前实现只提供通知机制，并没有自动暂停输入，所以更准确地说它是实现背压的基础，而不是完整背压策略。

## 2.7 连接建立、关闭与状态机

### 问题 26：shutdown 和 close 有什么区别？

**参考回答：**

`close` 释放 fd；如果存在多个 fd 引用或正在异步使用，语义更复杂。`shutdown(fd, SHUT_WR)` 是半关闭，表示本端不再发送数据，但仍可以接收对端数据。项目的 `shutdown` 先进入 disconnecting 状态，等输出缓冲区清空后再执行 `shutdownWrite`，防止响应数据被截断。

### 问题 27：优雅关闭和强制关闭如何实现？

**参考回答：**

优雅关闭调用 `shutdown`，等待待发送数据清空再关闭写端；强制关闭调用 `forceClose`，把操作投递到连接所属 EventLoop，直接进入关闭处理。两者都通过连接状态机和 EventLoop 线程归属避免重复关闭及并发修改。

### 问题 28：什么是 TIME_WAIT？哪一方进入 TIME_WAIT？

**参考回答：**

通常主动完成 TCP 关闭的一方进入 TIME_WAIT，持续约 2MSL，用于确保最后 ACK 可以重传并避免旧连接报文影响同一四元组的新连接。大量短连接可能产生大量 TIME_WAIT，因此 HTTP Keep-Alive 可以减少频繁建连和关闭。

### 问题 29：为什么要处理 EPIPE、ECONNRESET 和 SIGPIPE？

**参考回答：**

对端已经关闭时继续写可能得到 `EPIPE` 并触发 SIGPIPE，若未处理，进程可能被终止；连接被对端复位时会得到 `ECONNRESET`。项目在发送路径识别这些错误并关闭连接。生产环境还应在进程级忽略 SIGPIPE，或使用 `MSG_NOSIGNAL`；当前实现值得继续补强这一点。

### 问题 30：连接对象为什么需要状态机？

**参考回答：**

连接存在 connecting、connected、disconnecting、disconnected 等阶段。send、shutdown、forceClose 和对端关闭可能交错发生。状态机能限制每个操作的合法阶段，原子状态和幂等关闭可以避免重复回调、重复移除 Channel 或在断开后继续发送。

## 2.8 Channel 生命周期

### 问题 31：`Channel::tie` 解决什么问题？

**参考回答：**

epoll 返回活跃 Channel 后，对应 TcpConnection 可能已经在其他回调中被移除。Channel 保存 TcpConnection 的 `weak_ptr`，事件处理前尝试提升为 `shared_ptr` guard；提升成功才执行回调，并保证整个回调期间 TcpConnection 存活。使用 weak_ptr 是为了避免 Channel 与 TcpConnection 形成强引用环。

### 问题 32：为什么不能在 Channel 回调中立即销毁 Channel？

**参考回答：**

当前调用栈仍在执行 Channel 成员函数，直接销毁会造成 use-after-free。Connector 在移除 Channel 后，通过 `queueInLoop` 延迟到当前事件处理结束再 reset，正是为了避开重入和悬空对象问题。

## 2.9 定时器

### 问题 33：项目如何实现定时器？

**参考回答：**

每个 EventLoop 有一个 TimerQueue。TimerQueue 创建非阻塞 `timerfd` 并注册到该 loop。定时器按“过期时间 + 唯一序号”保存在有序集合中，最早时间变化时重设 timerfd。timerfd 可读后取出所有已到期定时器、执行回调，并重新插入仍需重复执行的定时器。

### 问题 34：为什么用 CLOCK_MONOTONIC？

**参考回答：**

单调时钟不受系统时间被人工调整或 NTP 校时的直接影响，适合计算相对超时。项目的 timerfd 使用 `CLOCK_MONOTONIC`。需要注意，当前 Timestamp 使用墙上时间表示目标时间，再计算相对间隔交给单调 timerfd；更严格的设计可以统一使用单调时间表示内部 deadline。

### 问题 35：为什么同时使用 set 和 unordered_map？

**参考回答：**

`set<unique_ptr<Timer>>` 按过期时间排序并拥有 Timer，便于找到最早定时器和批量取出过期项；`unordered_map<sequence, Timer*>` 不拥有对象，只用于根据 TimerId 快速定位并取消。二者通过断言保持数量一致。

### 问题 36：周期定时器在自己的回调中取消，怎么处理？

**参考回答：**

执行过期回调前，定时器已经从主集合取出，所以此时在 active map 中找不到。项目用 `callingExpiredTimers_` 标记回调阶段，并把此时取消的 sequence 放入 `cancelingTimers_`。reset 时只有重复且不在取消集合中的定时器才重新插入。

---

# 三、C++ 知识点与面试问答

## 3.1 RAII 与资源管理

### 问题 37：项目中 RAII 体现在哪里？

**参考回答：**

- `Socket` 析构时关闭 fd。
- `EPollPoller` 析构时关闭 epoll fd。
- `EventLoop` 和 `TimerQueue` 析构时移除 Channel 并关闭 eventfd、timerfd。
- `unique_ptr` 管理 Poller、Channel、Socket、Timer 等独占资源。
- `lock_guard`、`unique_lock` 管理互斥锁的加锁与释放。
- `EventLoopThread` 析构时通知 loop 退出并 join 线程。

RAII 的价值是把资源释放绑定到对象生命周期，即使发生早返回或异常，也更不容易泄漏资源。

### 问题 38：为什么很多类继承 noncopyable？

**参考回答：**

EventLoop、Socket、Channel、TcpServer 等对象绑定了 fd、线程或回调关系，复制会产生两个对象共同认为自己拥有同一资源，可能导致重复关闭和状态不一致。因此显式删除复制构造和复制赋值，让所有权约束在编译期体现。

## 3.2 智能指针与所有权

### 问题 39：项目中 unique_ptr、shared_ptr、weak_ptr 分别用在哪里？

**参考回答：**

- `unique_ptr`：明确独占所有权，例如 TcpConnection 独占 Socket 和 Channel，EventLoop 独占 Poller 和 TimerQueue。
- `shared_ptr`：TcpConnection 会同时被 TcpServer 连接表、事件回调和异步任务引用，因此需要共享生命周期。
- `weak_ptr`：Channel tie、HTTP 空闲定时器和 Session 清理回调不应延长业务对象生命周期，因此用 weak_ptr 检查对象是否仍存在。

### 问题 40：为什么 TcpConnection 继承 enable_shared_from_this？

**参考回答：**

TcpConnection 在成员函数中需要把自身安全地捕获到异步回调，`shared_from_this` 可以取得与外部控制块相同的 shared_ptr，保证回调执行期间对象存活。不能写 `shared_ptr<TcpConnection>(this)`，因为那会创建新的控制块，最终可能重复释放。

### 问题 41：shared_from_this 有什么使用前提？

**参考回答：**

对象必须已经由 shared_ptr 管理，否则调用 `shared_from_this` 会抛出 `bad_weak_ptr`。因此项目用 `make_shared<TcpConnection>` 和 `make_shared<Connector>` 创建这类对象，不在构造函数中调用 `shared_from_this`。

### 问题 42：项目如何避免 shared_ptr 循环引用？

**参考回答：**

Channel 到 TcpConnection 使用 weak_ptr；HTTP idle timer 捕获 weak_ptr；MemorySessionStorage 的周期回调捕获共享 State 的 weak_ptr。原则是：拥有关系使用强引用，观察或回调关系优先使用弱引用。

## 3.3 移动语义与值类别

### 问题 43：为什么 send 同时提供 string_view 和 string&& 重载？

**参考回答：**

在所属 EventLoop 线程内可以同步消费 `string_view`，不必复制；跨线程发送时，原始 view 指向的数据在任务执行前可能已经失效，因此必须复制或转移所有权。`string&&` 重载把字符串 move 到 lambda 中，保证异步执行时数据仍然有效，并减少一次复制。

### 问题 44：lambda 捕获 shared_ptr 有什么作用和风险？

**参考回答：**

按值捕获 shared_ptr 能延长对象到回调执行完成，避免悬空访问；风险是回调长期不执行会延迟资源释放，若对象又拥有回调还可能形成环。因此生命周期不应被延长的定时回调使用 weak_ptr。

### 问题 45：TimerQueue 为什么先 new Timer，再在 loop 中交给 unique_ptr？

**参考回答：**

跨线程任务保存在 `std::function<void()>` 中，而 C++17 的 std::function 要求目标可复制，直接按值捕获 move-only 的 unique_ptr 不可行。当前实现先创建裸指针，lambda 可复制地捕获该指针，进入 EventLoop 后立即用 unique_ptr 接管。更现代的替代方案是 C++23 的 `std::move_only_function`，或者调整任务封装支持 move-only callable。

## 3.4 std::function、回调与解耦

### 问题 46：为什么网络库大量使用回调？

**参考回答：**

网络层只负责连接和字节流，不应依赖具体业务协议。通过连接回调、消息回调、写完成回调、关闭回调把事件交给上层，TcpServer 可以复用于 Echo、HTTP 或其他协议。代价是回调链增加了生命周期和重入管理的复杂度。

### 问题 47：std::function 的成本是什么？

**参考回答：**

std::function 提供类型擦除，可能存在间接调用开销；可调用对象超过小对象优化容量时还可能动态分配。网络库的事件回调灵活性通常比这部分成本更重要，但极端低延迟场景可以考虑模板、function_ref 或自定义小函数优化。

## 3.5 std::any 与连接上下文

### 问题 48：为什么用 std::any 保存 HTTP 上下文？

**参考回答：**

TcpConnection 属于通用 TCP 层，不应该直接依赖 HttpContext。`std::any` 允许上层为每条连接附加任意协议状态，HTTP 层存入解析器、请求计数、最后活跃时间和定时器 ID，从而保持网络层与协议层解耦。

### 问题 49：std::any 的风险是什么？

**参考回答：**

类型检查从编译期推迟到运行期，错误的 `any_cast` 可能抛异常。项目在确认连接建立时设置上下文，并在部分位置使用指针形式的 any_cast 做检查。若协议类型固定，也可以用模板连接、variant 或显式基类获得更强的类型安全。

## 3.6 原子变量和内存序

### 问题 50：为什么连接状态使用 atomic？

**参考回答：**

send、shutdown、stop 等入口可能从非 I/O 线程调用，需要无数据竞争地读取或改变连接状态。真正涉及 Channel、Buffer 的操作仍会被投递到所属 EventLoop；atomic 主要保护轻量状态，而不是让整个连接对象变成可被任意线程直接并发操作。

### 问题 51：为什么很多状态使用 memory_order_relaxed？

**参考回答：**

这些原子值主要用于状态判断和幂等控制，不承担发布其他普通内存数据的同步责任；对象内部操作顺序主要由 EventLoop 的线程归属、互斥锁和任务队列保证，因此 relaxed 足以保证原子性。异步日志切换函数指针时则使用 release/acquire，因为需要发布后端初始化结果。

回答时不能说 relaxed 完全没有顺序，它仍受单个原子的 modification order 约束，只是不提供跨对象的同步关系。

## 3.7 容器和异构查找

### 问题 52：Router 和 Header 为什么使用自定义透明 Hash？

**参考回答：**

容器的 key 是 string，但查询通常拿到 string_view。透明 hash/equal 允许直接用 string_view 查找，减少为了查询临时构造 string 的分配。HTTP Header 还使用大小写不敏感的 hash 和 equal，因为字段名按协议语义不区分大小写。

### 问题 53：Timer set 的比较器为什么包含 sequence？

**参考回答：**

多个定时器可能拥有相同过期时间。如果比较器只比较 expiration，set 会认为它们是同一个 key，导致后插入失败。加入全局递增 sequence 后既保证严格弱序，也可以唯一标识定时器。

---

# 四、并发与操作系统知识点与面试问答

## 4.1 线程、互斥锁与条件变量

### 问题 54：EventLoopThread 启动时为什么需要条件变量？

**参考回答：**

EventLoop 是子线程栈上的局部对象。调用线程启动子线程后，必须等到 EventLoop 构造完成并发布地址，才能返回可用指针。`startLoop` 用条件变量等待 `loop_ != nullptr`，子线程设置 loop_ 后 notify。while 谓词用于防止虚假唤醒。

### 问题 55：condition_variable 为什么必须和锁及谓词一起使用？

**参考回答：**

条件变量只负责通知，不保存业务条件，可能发生虚假唤醒。共享条件必须由互斥锁保护，等待方在循环或谓词中重新检查条件，才能避免读到不一致状态或误认为条件已经满足。

### 问题 56：项目哪些地方可能发生数据竞争，如何避免？

**参考回答：**

- pending functors 由 mutex 保护。
- 每个 Channel 和连接 Buffer 只在所属 EventLoop 线程操作。
- 连接状态、EventLoop 退出标记等使用 atomic。
- MemorySessionStorage 的 map 使用 mutex。
- MySQL 连接池的空闲队列、连接数和停止状态由 mutex 与 condition_variable 保护。
- 异步日志前端 Buffer 交换由 mutex 保护，磁盘写入在锁外进行。

## 4.2 惊群与负载均衡

### 问题 57：当前连接如何分配到 I/O 线程？

**参考回答：**

EventLoopThreadPool 使用 round-robin 轮询，不考虑每个 loop 当前连接数和负载。它实现简单、分配通常较均匀，但当连接耗时差异大时不一定最优。可以改成最少连接数、采样负载或一致性哈希策略。

### 问题 58：什么是惊群？项目是否会遇到？

**参考回答：**

惊群是多个等待者因同一事件同时被唤醒，但只有少数能真正处理，造成无效调度。当前默认只有 base loop 的一个 Acceptor 监听端口，不存在多个 accept 线程竞争同一个监听 Socket 的典型惊群。如果使用多个 `SO_REUSEPORT` Socket，连接由内核分发，设计又不同。

## 4.3 文件描述符与系统资源

### 问题 59：什么是 fd？为什么要设置 CLOEXEC？

**参考回答：**

fd 是进程文件描述符表中的索引，Socket、epoll、eventfd、timerfd 都以 fd 表示。`CLOEXEC` 表示进程调用 exec 执行新程序时自动关闭该 fd，避免无意把网络连接和内部控制 fd 泄漏给子进程。

### 问题 60：accept 返回 EMFILE 怎么办？

**参考回答：**

EMFILE 表示进程 fd 达到上限。当前项目会记录错误并结束本轮 accept，但更成熟的做法是预留一个 idle fd：遇到 EMFILE 时先关闭 idle fd，accept 并立即关闭一个连接，再重新打开 idle fd，避免监听 fd 因 LT 持续可读造成 busy loop。同时应配合连接上限、监控和系统 ulimit。

## 4.4 I/O 线程不能做什么

### 问题 61：能否在消息回调中直接执行复杂 SQL 或耗时计算？

**参考回答：**

不建议。一个 sub loop 同时管理多条连接，回调阻塞会拖慢该 loop 上所有连接。应该把耗时任务投递到业务线程池，完成后再通过 `queueInLoop` 把发送响应的操作投递回连接所属 I/O 线程。

### 问题 62：如果业务线程完成任务时连接已经关闭怎么办？

**参考回答：**

业务任务应持有 weak_ptr 而不是无条件长期持有连接。完成后先 lock，成功说明对象仍存在，再把操作投递到其 EventLoop；真正发送时还要再次检查 connected 状态。这样避免向已断开的连接写入，也避免业务任务不必要地延长连接生命。

---

# 五、HTTP、路由、中间件与 Session

## 5.1 HTTP 增量解析

### 问题 63：HTTP 请求可能一次收不完整，项目怎么处理？

**参考回答：**

每条连接有独立 HttpContext，状态依次为请求行、Headers、Body 和 GotAll。数据不足时保留状态与 Buffer 中未消费数据，等待下一次读事件继续，而不是把半个请求判为错误。

### 问题 64：项目如何防止恶意超大请求？

**参考回答：**

解析器限制请求行 8KB、Headers 64KB、Body 64KB；Content-Length 必须是完整合法的无符号整数，超过限制或格式错误就返回 400 并关闭连接。这样可限制单连接内存占用。不过生产级服务还应加入请求总超时、慢速请求防护和全局连接限额。

### 问题 65：当前为什么不支持 chunked？如何扩展？

**参考回答：**

当前解析器发现 `Transfer-Encoding` 就拒绝请求，只支持 Content-Length 定界。要支持 chunked，需要增加 chunk size、chunk data、chunk CRLF、trailers 等解析状态，逐块消费 Buffer，校验十六进制长度和总大小，并防止同时出现冲突的 Content-Length 导致请求走私。

### 问题 66：如何处理 HTTP Pipeline？

**参考回答：**

`onMessage` 在 Buffer 仍有可读数据时循环解析。一个请求完成后立即路由并生成响应，再 reset HttpContext 继续解析下一请求，因此可以顺序处理同一连接中连续到达的多个请求，响应顺序与请求顺序一致。它不是 HTTP/2 的多路复用，慢 Handler 仍会阻塞后续 Pipeline 请求。

## 5.2 HTTP 长连接

### 问题 67：HTTP/1.0 与 HTTP/1.1 的 Keep-Alive 默认行为是什么？

**参考回答：**

HTTP/1.0 默认短连接，只有显式 `Connection: Keep-Alive` 才保持；HTTP/1.1 默认长连接，只有显式 `Connection: close` 才关闭。项目按大小写不敏感方式判断 Connection 头。

### 问题 68：项目如何清理空闲 HTTP 连接？

**参考回答：**

连接建立时设置一个 60 秒 idle timer，每次收到消息更新最后活跃时间。定时器触发后重新计算实际空闲时长：如果已经超时就 shutdown，否则按剩余时间重新调度。这种方式避免每个读事件都取消并创建新定时器。

### 问题 69：为什么 idle timer 捕获 weak_ptr？

**参考回答：**

如果定时器强引用 TcpConnection，连接即使已经从服务器移除，也可能因为定时器而继续存活到超时。weak_ptr 不拥有连接，回调触发时 lock，连接不存在就直接返回。

## 5.3 Router Trie

### 问题 70：为什么路由使用 Trie？

**参考回答：**

路由天然按 `/` 分段，Trie 可以共享公共前缀，匹配复杂度主要与路径段数有关，而不是逐条扫描全部路由。项目每个 HTTP Method 维护对应路由，路径节点支持静态、动态参数和通配符。

### 问题 71：`/user/list` 与 `/user/:id` 同时存在时匹配哪个？

**参考回答：**

优先匹配静态节点，然后动态参数，最后通配符。因此 `/user/list` 会命中静态路由，不会把 list 当作 id。递归匹配失败时会回溯参数列表。

### 问题 72：通配符有什么限制？

**参考回答：**

`*` 必须是最后一个路径段，用来收集剩余路径。项目也允许 `/static/*` 匹配 `/static/`，此时通配符值为空。注册重复完整路由会抛出异常。

## 5.4 中间件与 CORS

### 问题 73：中间件责任链怎样工作？

**参考回答：**

请求阶段按注册顺序调用 `before`；某个 before 返回 false 时停止后续路由，但仍执行响应阶段。响应阶段按逆序调用 `after`，形成类似洋葱模型。after 的异常会被捕获并记录，避免单个中间件破坏整个响应流程。

### 问题 74：为什么 after 要逆序执行？

**参考回答：**

假设中间件 A 先进入、B 后进入，退出时应该 B 先收尾、A 后收尾，类似函数调用栈和 RAII。认证、计时、压缩等中间件可以正确包裹内层处理过程。

### 问题 75：什么是 CORS？预检请求是什么？

**参考回答：**

CORS 是浏览器执行的跨源访问控制机制。对于非简单跨源请求，浏览器先发送 OPTIONS 预检，询问允许的 Origin、Method 和 Headers。服务端通过 `Access-Control-Allow-*` 响应头声明策略。CORS 不是服务端鉴权机制，非浏览器客户端不会替服务端执行这套限制。

## 5.5 Cookie 与 Session

### 问题 76：Cookie 和 Session 有什么区别？

**参考回答：**

Cookie 存在客户端并随请求发送；Session 数据主要存在服务端，客户端 Cookie 只保存 sessionId。项目的 SessionManager 从 Cookie 取 sessionId，再到 SessionStorage 查找 Session。

### 问题 77：项目如何生成 Session ID？安全吗？

**参考回答：**

使用 Linux `getrandom` 获取 16 字节随机数，再编码为 32 个十六进制字符，提供 128 位随机空间。Cookie 默认设置 HttpOnly 和 SameSite=Lax，可配置 Secure。登录或提权时调用 createSession 生成新 ID，可降低会话固定攻击风险。

### 问题 78：HttpOnly、Secure、SameSite 分别是什么？

**参考回答：**

- HttpOnly：禁止 JavaScript 通过 document.cookie 读取，降低 XSS 窃取 Cookie 的风险，但不能防止 XSS 代替用户发请求。
- Secure：只通过 HTTPS 发送。
- SameSite：限制跨站请求携带 Cookie，降低 CSRF 风险。Lax 是安全性与可用性的折中。

### 问题 79：内存 Session 存储有什么局限？

**参考回答：**

进程重启会丢失，单机内存容量有限，多实例之间不共享。当前用 mutex 保护 map，并支持定时清理过期项。生产环境可以实现同一 SessionStorage 接口接入 Redis，同时考虑序列化、TTL、并发更新和故障降级。

---

# 六、日志、数据库与工程化

## 6.1 异步日志

### 问题 80：异步日志为什么能降低 I/O 线程延迟？

**参考回答：**

前端线程只把格式化后的日志追加到内存 Buffer，Buffer 满或定期刷新时交给后台线程，磁盘 append 和 flush 在后台完成。这样网络线程通常不直接等待磁盘 I/O。当前实现前端仍需要短时间加锁，所以不能称为无锁。

### 问题 81：双缓冲思想是什么？

**参考回答：**

前端有 current buffer 和备用 next buffer；后台也准备可复用 Buffer。交换后前端立刻获得新 Buffer 继续写，后台在锁外批量落盘，写完的 Buffer 清空复用，减少频繁分配和长时间持锁。

### 问题 82：日志生产速度长期高于消费速度怎么办？

**参考回答：**

无限缓存会导致 OOM，因此项目设置最大排队 Buffer 数量，超过后丢弃日志并增加 droppedLogs 计数。生产系统应监控该计数；对于审计类不可丢日志，需要采用不同的可靠性策略，而不是沿用普通调试日志策略。

### 问题 83：为什么 Logger 在析构时输出？

**参考回答：**

宏创建一个临时 Logger，连续的 `operator<<` 把内容写入 LogStream；完整表达式结束时临时对象析构，统一补充文件名、函数和行号，再一次输出。这利用了 RAII 和临时对象生命周期。

## 6.2 MySQL 连接池

### 问题 84：为什么需要数据库连接池？

**参考回答：**

建立数据库连接涉及 TCP、认证和资源初始化，频繁创建销毁成本高。连接池复用连接，并通过最大连接数限制数据库压力。项目支持初始连接数、最大连接数、获取超时、使用前 ping、失败重连和后台健康检查。

### 问题 85：连接池如何归还连接？

**参考回答：**

对外返回 `unique_ptr<DbConnection, custom_deleter>`。调用方离开作用域后，自定义删除器不是直接 delete，而是清理连接事务状态并归还空闲队列；如果连接池正在关闭或连接失效，则销毁并减少总连接数。这也是 RAII 的应用。

### 问题 86：达到最大连接数时怎么办？

**参考回答：**

调用线程在条件变量上等待，直到有连接归还、允许创建新连接或连接池关闭；超过 acquireTimeout 则抛出异常，防止无限等待。要注意：如果在 I/O 线程中同步等待数据库连接，仍会阻塞 Reactor，所以业务层最好在独立线程池使用数据库连接池。

### 问题 87：连接池为什么需要健康检查？

**参考回答：**

连接可能被数据库、网络设备或超时策略关闭，但客户端暂时不知道。定期检查和使用前 ping 可以淘汰无效连接；同时可按最大空闲时间回收多余连接，避免长期占用数据库资源。

## 6.3 CMake 与模块化

### 问题 88：项目如何组织构建？

**参考回答：**

根 CMake 使用 C++17 构建 `libmymuduo.so`，头文件通过 build/install interface 暴露；示例、测试、日志行为和 MySQL 模块都有独立选项。MySQL 默认关闭，启用时才查找并链接 Connector/C++，避免基础网络库强制依赖数据库环境。

### 问题 89：为什么链接时使用 `--no-undefined`？

**参考回答：**

它要求生成共享库时不能留下未解析符号，可以在构建阶段更早发现漏链接或实现缺失，而不是等应用加载共享库后才报错。

## 6.4 测试与动态分析

### 问题 90：项目测试覆盖哪些内容？

**参考回答：**

当前有 15 个不依赖 gtest 的测试，覆盖 Buffer、Timestamp、TimerQueue、EventLoop、EventLoopThread、线程池、TCP Echo、多客户端、大消息、客户端重试、定时器取消边界、forceClose、shutdown、客户端销毁以及多客户端往返通信。

### 问题 91：ASan、UBSan、TSan、Valgrind 分别检查什么？

**参考回答：**

- ASan：越界、use-after-free、部分内存错误。
- UBSan：未定义行为，例如有符号溢出、非法类型操作。
- TSan：线程数据竞争。
- Valgrind Memcheck：内存泄漏、非法访问、未初始化值等，通常更慢。

工具不能替代测试设计。尤其 TSan 只能发现实际执行路径上的竞争，网络异常、重入和生命周期边界仍要用针对性用例覆盖。

### 问题 92：如何为网络库设计可靠测试？

**参考回答：**

除了正常 Echo，还要覆盖：消息分片与合并、超过 Buffer 初始容量的大包、客户端突然断开、服务端半关闭、重复关闭、连接失败重试、定时器在回调中取消、多客户端并发、服务器或客户端提前析构、慢读导致输出积压。测试端口应动态分配，避免固定端口冲突，并设置超时防止测试永久挂起。

---

# 七、项目设计类综合问题

## 问题 93：请用两分钟介绍这个项目

**参考回答：**

这是我用 C++17 实现的 Linux 网络库，并在其上实现了 HTTP 服务框架。底层采用主从 Reactor 和 one loop per thread 模型：base loop 负责 accept，连接轮询分配给 sub loop；EventLoop 基于 epoll 等待事件，Channel 封装 fd 和回调，跨线程任务通过 eventfd 唤醒。TCP 层处理了非阻塞连接、输入输出 Buffer、部分写、高水位回调、优雅关闭和客户端指数退避重连；timerfd 提供定时任务。

在此基础上，我实现了 HTTP 增量状态机、HTTP/1.0/1.1 长连接、Pipeline 顺序处理、Trie 路由、中间件、CORS、Cookie 和 Session，还增加了异步日志与可选 MySQL 连接池。项目当前有 15 项测试，并配置了 Sanitizer 与 Valgrind 检查流程。这个项目最大的收获是理解了线程归属、异步回调和对象生命周期之间的关系，而不只是会调用 Socket API。

## 问题 94：这个项目最难的部分是什么？

**参考回答：**

最难的是跨线程场景下的连接生命周期。TcpServer 的连接表在 base loop，连接 I/O 在 sub loop，业务又可能从其他线程调用 send。如果直接跨线程操作 Channel 或销毁连接，很容易出现数据竞争和 use-after-free。我的处理是让每条连接固定归属一个 EventLoop，跨线程操作只投递任务；TcpConnection 用 shared_ptr 保证异步任务期间存活，Channel 用 weak_ptr tie 防止回调对象提前销毁，关闭流程分别在 base loop 和 sub loop 完成连接表删除与 Channel 移除。

## 问题 95：为什么说项目是高性能设计？是否有数据证明？

**参考回答：**

项目采用了非阻塞 I/O、epoll、I/O 线程分片、按需监听 EPOLLOUT、readv、异步日志和 Buffer 复用，这些都是降低阻塞、系统调用和分配开销的设计。但目前没有提交可复现的压测报告，所以我会称它为“面向高并发场景设计”，不会声称达到具体 QPS。下一步会补充固定环境下的 wrk/自研客户端基准、延迟分位数、CPU 和内存数据。

## 问题 96：如果要做压测，你会测什么？

**参考回答：**

至少记录：

- 测试机器 CPU、内存、内核、编译优化选项、线程数和 fd 上限。
- 不同并发连接数、请求大小、长短连接比例。
- 吞吐量、平均延迟、P50/P95/P99/P999、错误率。
- CPU 利用率、上下文切换、系统调用、RSS、网络带宽。
- 单线程与多线程扩展性、同步与异步日志差异。
- 慢客户端、大响应、连接抖动等压力场景。

工具可以使用 wrk、wrk2、perf、pidstat、ss 和 flame graph。压测必须给出完整命令和配置，保证结果可复现。

## 问题 97：如果有 10 万长连接，首先要关注什么？

**参考回答：**

首先不是只调 epoll，而是系统性检查：进程和系统 fd 上限、每连接内存、内核 TCP 参数、listen backlog、网卡与带宽、线程数、空闲连接回收、日志量、定时器规模和应用协议心跳。当前每连接包含两个初始约 1KB Buffer，加上对象、Channel、Socket、容器节点等，必须实测单连接内存。HTTP 当前每连接一个独立 idle timer，连接很多时也要评估 TimerQueue 的 O(log N) 操作和定时器内存。

## 问题 98：如果一个 sub loop 特别忙怎么办？

**参考回答：**

当前轮询分配只保证连接数大致均匀，不保证请求负载均匀。可以监控每个 loop 的活跃连接数、事件处理耗时和任务队列长度，改为 power-of-two choices 或最小负载分配。更重要的是识别耗时回调并迁移到业务线程池，避免 I/O loop 被业务阻塞。

## 问题 99：网络库如何保证线程安全？

**参考回答：**

项目不是给所有对象都加锁，而是采用“线程约束 + 少量同步”：Channel、Poller、Buffer 在所属 EventLoop 单线程操作；跨线程请求通过任务队列迁移；队列本身用 mutex；轻量状态用 atomic；共享 Session 和连接池再局部加锁。这样热路径锁更少，也让并发关系更清晰。

## 问题 100：你参考了 Muduo，项目价值在哪里？

**参考回答：**

我会坦诚说明架构思想参考了 Muduo，价值不在于发明 Reactor，而在于亲自实现和验证其关键机制，并继续使用 C++17 标准库扩展 HTTP 状态机、Trie 路由、中间件、Session、可选数据库连接池、日志配置及测试流程。面试时重点说明自己真正理解并解决过的边界问题，例如跨线程唤醒、部分写、Channel 生命周期和定时器回调内取消。

---

# 八、缺陷、边界与改进方向

面试官往往会通过“你的项目还有什么问题”判断候选人是否真正理解工程边界。不要回答“目前没什么问题”。

## 8.1 当前已知边界

1. **HTTP 协议不完整**：不支持 chunked、TLS、WebSocket、HTTP/2，也没有流式上传。
2. **业务隔离不足**：提供 I/O 线程池，但没有通用业务线程池；耗时 Handler 会阻塞 sub loop。
3. **压测数据缺失**：还不能用客观数据说明吞吐、尾延迟和扩展性。
4. **背压只是基础能力**：有 output buffer 和 high-water callback，但没有默认的自动暂停读策略。
5. **EMFILE 处理不完整**：记录错误，但未实现 idle fd 技巧。
6. **SIGPIPE 防护需明确**：发送路径处理 EPIPE，但还应统一忽略 SIGPIPE 或使用 MSG_NOSIGNAL。
7. **路由注册期线程安全**：Router 更适合启动前完成注册；运行期间动态注册需要额外同步或不可变快照。
8. **内存 Session 不支持分布式**：多实例需要 Redis 等外部存储。
9. **时间体系可统一**：内部 deadline 最好统一采用 steady/monotonic clock。
10. **可观测性不足**：缺少连接数、队列深度、事件延迟、丢日志数等指标导出。

## 8.2 问题 101：如果继续迭代，你优先做什么？

**参考回答：**

我会先做三件事：第一，补充可复现 benchmark 和 perf flame graph，用数据找瓶颈；第二，加入业务线程池、完善背压和全链路超时，避免慢业务拖住 Reactor；第三，补齐生产可靠性，包括 SIGPIPE、EMFILE、连接上限、监控指标和优雅停机。协议功能扩展如 chunked 和 TLS 可以在可靠性之后推进。

## 8.3 问题 102：怎样支持优雅停机？

**参考回答：**

停止接收新连接；标记服务器进入 draining；让已有连接完成在途请求，禁止或限制新请求；设置最大排空时间；等待输出 Buffer 清空后半关闭；最终强制关闭剩余连接并停止各 EventLoop；最后 flush 日志和关闭数据库连接池。整个过程要避免新任务继续进入已经退出的 loop。

## 8.4 问题 103：怎样实现更完整的背压？

**参考回答：**

当输出 Buffer 超过高水位时，暂停该连接的读事件或暂停上游业务生产；低于低水位时再恢复读。高低两个阈值可以防止频繁抖动。还应设置单连接最大输出缓存，超过时选择丢弃、返回错误或断开，并配合全局内存预算防止许多慢客户端拖垮进程。

## 8.5 问题 104：怎样避免慢 HTTP 攻击？

**参考回答：**

除了报文大小限制，还应设置请求行超时、Header 超时、Body 超时、最小传输速率、单 IP 连接数限制和全局连接上限。当前 idle timer 按“收到任何数据就更新”会被极慢的数据流持续续期，因此还需要一个从请求开始计算、不因少量数据无限延长的绝对请求超时。

---

# 九、面试自述模板

## 9.1 30 秒简版

我实现了一个基于 C++17 和 Linux epoll 的网络库，采用主从 Reactor 与 one loop per thread 模型，通过 eventfd 处理跨线程唤醒，通过 timerfd 实现定时任务。TCP 层支持服务端、客户端、非阻塞连接、Buffer、部分写、优雅关闭和重试；上层实现了 HTTP 增量解析、长连接、Trie 路由、中间件、Session、异步日志及可选 MySQL 连接池，并编写了 15 项测试覆盖连接和定时器边界。

## 9.2 主要工作表述

- 实现 EventLoop、Channel、EPollPoller 等 Reactor 核心组件，并通过 eventfd 完成线程安全的任务投递。
- 实现主从 Reactor TCP Server/Client、线程池和连接状态机，处理非阻塞 I/O、部分写、连接重试及生命周期问题。
- 基于 timerfd 和有序 TimerQueue 实现一次性、周期性和可取消定时任务。
- 实现 HTTP/1.1 增量解析、Keep-Alive、Pipeline、Trie 路由、Middleware、CORS、Cookie 与 Session。
- 实现异步日志和可选 MySQL 连接池，并建立 Sanitizer、Valgrind 和多场景网络测试流程。

## 9.3 项目难点表述

项目最难的是异步回调下的线程安全和对象生命周期。我采用每个连接固定归属一个 EventLoop 的方式约束并发访问，其他线程只能投递操作；再结合 shared_ptr、weak_ptr、Channel tie 和连接状态机，保证连接关闭、回调执行和服务器移除连接之间不会出现明显的悬空访问或重复关闭。

## 9.4 个人收获表述

这个项目让我从会使用 Socket API，进阶到理解事件驱动服务器的完整运行机制，包括 epoll 事件分发、线程间唤醒、非阻塞读写、连接生命周期、定时器和应用层协议状态机。同时也加深了我对 RAII、智能指针、移动语义、原子内存序以及并发测试的理解。

---

# 十、面试前速查清单

面试前应能不看代码回答以下问题：

## 网络编程

- [ ] 能画出 EventLoop、Channel、Poller、TcpConnection 的关系。
- [ ] 能说明 base loop 与 sub loop 的职责。
- [ ] 能解释为什么当前是 LT，以及改 ET 需要改什么。
- [ ] 能解释非阻塞 connect 的 EINPROGRESS 与 SO_ERROR。
- [ ] 能解释为什么 EPOLLOUT 按需开启。
- [ ] 能讲清部分写、EAGAIN、EPIPE、ECONNRESET。
- [ ] 能讲清 TCP 粘包不是 TCP 错误，而是应用层边界问题。
- [ ] 能说明 eventfd、timerfd、readv 的作用。
- [ ] 能说明 shutdownWrite 与 close 的区别。
- [ ] 能完整描述连接建立和关闭流程。

## C++ 与并发

- [ ] 能说明 unique_ptr/shared_ptr/weak_ptr 的使用边界。
- [ ] 能解释 enable_shared_from_this 的前提与错误用法。
- [ ] 能说明 Channel tie 防止的具体问题。
- [ ] 能说明为什么跨线程 string_view 需要复制。
- [ ] 能说明 atomic relaxed 为什么在当前状态变量上可用。
- [ ] 能说明条件变量为什么需要谓词。
- [ ] 能说明任务队列为什么 swap 后锁外执行。
- [ ] 能说明 RAII 在 fd、锁、线程和连接池中的应用。

## HTTP 与工程化

- [ ] 能画出 HTTP 解析状态机。
- [ ] 能说明 HTTP/1.0 和 1.1 的长连接差异。
- [ ] 能说明 Pipeline 与 HTTP/2 多路复用的区别。
- [ ] 能说明 Trie 路由匹配优先级。
- [ ] 能说明中间件 before/after 的执行顺序。
- [ ] 能说明 Cookie、Session、HttpOnly、Secure、SameSite。
- [ ] 能主动说明 chunked、TLS、业务线程池等当前边界。
- [ ] 能说明异步日志为什么不是无锁，也不是绝不丢日志。
- [ ] 能说明数据库连接池不能消除 I/O 线程阻塞。
- [ ] 能设计一套可复现的压测与性能分析方案。

## 最后提醒

1. 回答先讲设计目标，再讲当前实现，最后讲边界和改进。
2. 不要只背 API，要能沿一次连接或一次请求讲出完整数据流。
3. 不确定的性能结果明确说尚未测试，并给出验证方案。
4. 对参考 Muduo 保持坦诚，把重点放在自己实现、调试和扩展的内容上。
5. 面试官追问 bug 时，优先从线程归属、对象生命周期、回调重入、部分读写和异常关闭五个方向分析。
