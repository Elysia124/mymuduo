#pragma once

#include "httpserver/http/HttpContext.h"
#include "httpserver/router/Router.h"
#include "httpserver/session/SessionManager.h"
#include "net/Buffer.h"
#include "net/Callbacks.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/TcpConnection.h"
#include "net/TcpServer.h"
#include "net/TimerId.h"
#include "net/Timestamp.h"
#include "net/noncopyable.h"
#include <functional>
#include <memory>
#include <string_view>
#include <utility>


namespace mymuduo::http {
class HttpRequest;
class HttpResponse;
class HttpServer : noncopyable
{
public:
    using HttpCallback = std::function<void(const HttpRequest&, HttpResponse*)>;
    using HandlerCallback = router::Router::HandlerCallback;
    using HandlerPtr = router::Router::HandlerPtr;

    HttpServer(EventLoop* loop, const InetAddress& listenAddr, std::string nameArg);

    void setHttpCallback(HttpCallback cb) { httpCallback = std::move(cb); }

    void setNumThreads(int numThreads) { server_.setThreadNum(numThreads); }

    void start();

    void Get(std::string_view path, HandlerCallback cb) { router_.Get(path, std::move(cb)); }
    void Get(std::string_view path, HandlerPtr cb) { router_.Get(path, std::move(cb)); }

    void Post(std::string_view path, HandlerCallback cb) { router_.Post(path, std::move(cb)); }
    void Post(std::string_view path, HandlerPtr handler) { router_.Post(path, std::move(handler)); }

    void Put(std::string_view path, HandlerCallback cb) { router_.Put(path, std::move(cb)); }
    void Put(std::string_view path, HandlerPtr handler) { router_.Put(path, std::move(handler)); }

    void Delete(std::string_view path, HandlerCallback cb) { router_.Delete(path, std::move(cb)); }
    void Delete(std::string_view path, HandlerPtr handler) { router_.Delete(path, std::move(handler)); }

    void Head(std::string_view path, HandlerCallback cb) { router_.Head(path, std::move(cb)); }
    void Head(std::string_view path, HandlerPtr handler) { router_.Head(path, std::move(handler)); }

    void Options(std::string_view path, HandlerCallback cb) { router_.Options(path, std::move(cb)); }
    void Options(std::string_view path, HandlerPtr handler) { router_.Options(path, std::move(handler)); }

    void setSessionManager(std::unique_ptr<session::SessionManager> manager)
    {
        sessionManager_ = std::move(manager);
    }
    session::SessionManager* getSessionManager() { return sessionManager_.get(); }

private:
    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime);
    void onRequest(const TcpConnectionPtr& conn, const HttpRequest& req);

    void updateLastActiveTime(const TcpConnectionPtr& conn, Timestamp now);
    void startIdleTimer(const TcpConnectionPtr& conn);
    void cancelIdleTimer(const TcpConnectionPtr& conn);
    static void handleIdleTimeout(const std::weak_ptr<TcpConnection>& weakConn);

    static void defaultHttpCallback(const HttpRequest&, HttpResponse*);

    TcpServer server_;
    router::Router router_;
    HttpCallback httpCallback;

    struct HttpConnectionContext
    {
        HttpContext context;
        std::size_t requestCount = 0;   // 每条连接最大请求数
        Timestamp lastActiveTime;
        TimerId idleTimer;   // idle timeout
    };

    static constexpr std::size_t kMaxRequestsPerConnection = 100000;   // 每条 HTTP 连接默认最大请求数
    static constexpr double kKeepAliveIdleTimeout = 60.0;              // 默认超时时间

    std::unique_ptr<session::SessionManager> sessionManager_;
};
}   // namespace mymuduo::http