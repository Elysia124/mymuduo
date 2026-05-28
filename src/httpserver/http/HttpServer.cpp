#include "httpserver/http/HttpServer.h"
#include "httpserver/http/HttpContext.h"
#include "httpserver/http/HttpRequest.h"
#include "httpserver/http/HttpResponse.h"
#include "net/Buffer.h"
#include "net/Logger.h"
#include "net/TcpConnection.h"
#include "net/TimerId.h"
#include "net/Timestamp.h"
#include <algorithm>
#include <any>
#include <cctype>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

using namespace mymuduo::http;

namespace {

bool equalsCaseInsensitive(std::string_view lhs, std::string_view rhs)
{
    auto to_lower_proj = [](unsigned char c) { return std::tolower(c); };
    return lhs.size() == rhs.size() &&
           std::ranges::equal(lhs, rhs, std::ranges::equal_to{}, to_lower_proj, to_lower_proj);
}

bool shouldCloseConnection(const HttpRequest& req)
{
    auto connection = req.getHeader("Connection");
    if (req.version() == HttpRequest::Version::kHttp10) {
        // HTTP1.0 默认短链接，除非显示指定 Keep-Alive
        return !(connection.has_value() && equalsCaseInsensitive(connection.value(), "Keep-Alive"));
    }
    if (req.version() == mymuduo::http::HttpRequest::Version::kHttp11) {
        // HTPP1.1 默认长连接，除非显示指定 close
        return connection.has_value() && equalsCaseInsensitive(connection.value(), "close");
    }

    return true;
}
}   // namespace

HttpServer::HttpServer(EventLoop* loop, const InetAddress& listenAddr, std::string nameArg)
    : server_(loop, listenAddr, std::move(nameArg)), httpCallback(defaultHttpCallback)
{
    server_.setConnectionCallback([this](const TcpConnectionPtr& conn) { onConnection(conn); });
    server_.setMessageCallback([this](const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {
        onMessage(conn, buf, receiveTime);
    });
}
void HttpServer::start()
{
    server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr& conn)
{
    if (conn->connected()) {
        HttpConnectionContext ctx;
        ctx.lastActiveTime = Timestamp::now();
        conn->setContext(std::move(ctx));
        startIdleTimer(conn);
    }
    else {
        cancelIdleTimer(conn);
    }
}

void HttpServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime)
{
    // 每次收到消息就重置时间，确保不会在收到HTTP消息时断连
    updateLastActiveTime(conn, receiveTime);

    auto& httpConnCtx = std::any_cast<HttpConnectionContext&>(conn->getMutableContext());
    auto& context = httpConnCtx.context;
    while (buf->readableBytes() > 0) {
        if (!context.parseRequest(buf, receiveTime)) {
            LOG_ERROR << "HTTP close by parse error conn=" << conn->name().c_str()
                  << " readable=" << buf->readableBytes();
                  
            conn->send("HTTP/1.1 400 Bad Request\r\n"
                       "Connection: close\r\n"
                       "Content-Length: 0\r\n"
                       "\r\n");
            cancelIdleTimer(conn);
            conn->shutdown();
            return;
        }

        if (context.gotAll()) {
            onRequest(conn, context.request());
            context.reset();
        }
        else {
            break;
        }
    }
}

void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req)
{
    auto& httpConnCtx = std::any_cast<HttpConnectionContext&>(conn->getMutableContext());
    ++httpConnCtx.requestCount;

    bool close = shouldCloseConnection(req);

    if (httpConnCtx.requestCount >= kMaxRequestsPerConnection) {
        // 当前请求数大于等于每条HTTP连接的最大请求数，就关闭连接
        close = true;
        LOG_ERROR << "HTTP close by max requests conn=" << conn->name().c_str()
                  << " request_count=" << httpConnCtx.requestCount << " max=" << kMaxRequestsPerConnection;
    }

    HttpResponse response(close);

    if (!router_.route(req, &response)) {
        httpCallback(req, &response);
    }


    Buffer buf;
    const bool sendBody = req.method() != HttpRequest::Method::kHead;
    response.appendToBuffer(&buf, sendBody);

    conn->send(&buf);

    if (response.closeConnection()) {
        cancelIdleTimer(conn);
        conn->shutdown();
    }
}

void HttpServer::updateLastActiveTime(const TcpConnectionPtr& conn, Timestamp now)
{
    auto& anyCtx = conn->getMutableContext();
    if (!anyCtx.has_value()) {
        return;
    }

    auto* httpConnCtx = std::any_cast<HttpConnectionContext>(&anyCtx);
    if (!httpConnCtx) {
        return;
    }

    httpConnCtx->lastActiveTime = now;
}

void HttpServer::startIdleTimer(const TcpConnectionPtr& conn)
{
    auto& anyCtx = conn->getMutableContext();
    if (!anyCtx.has_value()) {
        return;
    }

    auto* httpConnCtx = std::any_cast<HttpConnectionContext>(&anyCtx);
    if (!httpConnCtx) {
        return;
    }

    if (httpConnCtx->idleTimer.valid()) {   // 取消当前的定时器
        conn->getLoop()->cancel(httpConnCtx->idleTimer);
    }

    std::weak_ptr<TcpConnection> weakConn(conn);

    // 设置新的定时器，在 kKeepAliveIdleTimeout 秒后强制关闭连接
    httpConnCtx->idleTimer =
        conn->getLoop()->runAfter(kKeepAliveIdleTimeout, [weakConn] { handleIdleTimeout(weakConn); });
}

void HttpServer::cancelIdleTimer(const TcpConnectionPtr& conn)
{
    auto& anyCtx = conn->getMutableContext();
    if (!anyCtx.has_value()) {
        return;
    }

    auto* httpConnCtx = std::any_cast<HttpConnectionContext>(&anyCtx);
    if (!httpConnCtx) {
        return;
    }

    if (httpConnCtx->idleTimer.valid()) {
        conn->getLoop()->cancel(httpConnCtx->idleTimer);
        httpConnCtx->idleTimer = TimerId();
    }
}

void HttpServer::handleIdleTimeout(const std::weak_ptr<TcpConnection>& weakConn)
{
    auto conn = weakConn.lock();
    if (!conn) {
        return;
    }

    auto& anyCtx = conn->getMutableContext();
    if (!anyCtx.has_value()) {
        return;
    }

    auto* httpConnCtx = std::any_cast<HttpConnectionContext>(&anyCtx);
    if (!httpConnCtx) {
        return;
    }

    // 当前 HTTP 连接已经持续的时间(微秒)
    const auto idleUs =
        Timestamp::now().microSecondsSinceEpoch() - httpConnCtx->lastActiveTime.microSecondsSinceEpoch();

    // 超时时间(微秒)
    const auto timeoutUs = static_cast<int64_t>(kKeepAliveIdleTimeout * Timestamp::kMicroSecondsPerSecond);

    // 当前 HTTP 连接已经超时，则关闭连接
    if (idleUs >= timeoutUs) {
        LOG_ERROR << "HTTP close by idle timeout conn=" << conn->name().c_str()
                  << " request_count=" << httpConnCtx->requestCount << " idle_ms=" << idleUs / 1000;
        httpConnCtx->idleTimer = TimerId();
        conn->shutdown();
        return;
    }

    // 当前 HTTTP 连接的剩余时间(秒)
    const auto remainSeconds = static_cast<double>(timeoutUs - idleUs) / Timestamp::kMicroSecondsPerSecond;

    std::weak_ptr<TcpConnection> weakAgain(weakConn);
    httpConnCtx->idleTimer =
        conn->getLoop()->runAfter(remainSeconds, [weakAgain] { handleIdleTimeout(weakAgain); });
}


void HttpServer::defaultHttpCallback(const HttpRequest&, HttpResponse* resp)
{
    resp->setStatusCode(HttpResponse::HttpStatusCode::k404NotFound);
    resp->setStatusMessage("Not Found");
    resp->setContentType("text/plain");
    resp->setBody("404 Not Found\n");
}