#include "httpserver/http/HttpRequest.h"
#include "httpserver/http/HttpResponse.h"
#include "httpserver/http/HttpServer.h"
#include "httpserver/middleware/Middleware.h"
#include "httpserver/middleware/cors/CorsMiddleware.h"
#include "httpserver/session/MemorySessionStorage.h"
#include "httpserver/session/SessionManager.h"
#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "net/Logger.h"
#include "net/TimerQueue.h"
#include <memory>
#include <string>
#include <utility>

using namespace mymuduo;
using namespace mymuduo::http;

class ServerHeaderMiddleware : public middleware::Middleware
{
public:
    void after(const HttpRequest&, HttpResponse& resp) override { resp.setHeader("Server", "mymuduo"); }
};

int main()
{
    Logger::setLogLevel(Logger::INFO);
    EventLoop loop;
    InetAddress listenAddr(8888);
    HttpServer server(&loop, listenAddr, "HttpServer");
    server.addMiddleware(std::make_shared<ServerHeaderMiddleware>());
    server.addMiddleware(std::make_shared<middleware::CorsMiddleware>());

    auto Storage = std::make_unique<session::MemorySessionStorage>();
    Storage->setClearExpiredRegularly(&loop);

    server.setSessionManager(std::make_unique<session::SessionManager>(std::move(Storage), 3600));

    server.Get("/", [](const HttpRequest& req, const router::PathParams& params, HttpResponse* resp) {
        resp->setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/plain");
        resp->setBody("hello mymuduo\n");
    });

    server.Get("/users/:id",
               [](const HttpRequest& req, const router::PathParams& params, HttpResponse* resp) {
                   auto id = params.get("id");

                   resp->setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
                   resp->setStatusMessage("OK");
                   resp->setContentType("text/plain");

                   if (id.has_value()) {
                       resp->setBody("user id = " + std::string(id.value()) + "\n");
                   }
                   else {
                       resp->setBody("missing user id\n");
                   }
               });

    server.Get("/static/*", [](const HttpRequest& req, const router::PathParams& params, HttpResponse* resp) {
        auto path = params.get("*");

        resp->setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/plain");

        if (path.has_value()) {
            resp->setBody("static path = " + std::string(path.value()) + "\n");
        }
        else {
            resp->setBody("missing static path\n");
        }
    });

    server.Post("/name/:name",
                [](const HttpRequest& req, const router::PathParams& params, HttpResponse* resp) {
                    auto name = params.get("name");
                    const auto& body = req.body();
                    auto age = req.getQueryParam("age");

                    resp->setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
                    resp->setStatusMessage("OK");
                    resp->setContentType("text/plain");
                    resp->setBody("path name = " + std::string(name.value_or("null")) + "\n" +
                                  "body = " + body + "\n" + "age = " + (age.value_or("null")) + "\n");
                });

    server.Post("/login",
                [&server](const HttpRequest& req, const router::PathParams& params, HttpResponse* resp) {
                    server.getSessionManager()->destroySession(req, nullptr);
                    auto session = server.getSessionManager()->createSession(resp);

                    session->setValue("userId", "1001");
                    session->setValue("username", "ciao");
                    session->setValue("isLoggedIn", "true");

                    resp->setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
                    resp->setStatusMessage("OK");
                    resp->setContentType("text/plain");
                    resp->setBody("login success\n");
                });

    server.Get("/profile",
               [&server](const HttpRequest& req, const router::PathParams& params, HttpResponse* resp) {
                   auto session = server.getSessionManager()->findSession(req, resp);

                   if (!session || session->getValue("isLoggedIn") != "true") {
                       resp->setStatusCode(HttpResponse::HttpStatusCode::k401Unauthorized);
                       resp->setStatusMessage("Unauthorized");
                       resp->setContentType("text/plain");
                       resp->setBody("please login first\n");
                       return;
                   }

                   std::string username = session->getValue("username").value_or("");

                   resp->setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
                   resp->setStatusMessage("OK");
                   resp->setContentType("text/plain");
                   resp->setBody("hello " + username + "\n");
               });

    server.Post("/logout",
                [&server](const HttpRequest& req, const router::PathParams& params, HttpResponse* resp) {
                    server.getSessionManager()->destroySession(req, resp);

                    resp->setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
                    resp->setStatusMessage("OK");
                    resp->setContentType("text/plain");
                    resp->setBody("logout success\n");
                });

    server.Get("/cookie", [](const HttpRequest& req, const router::PathParams& params, HttpResponse* resp) {
        auto sid = req.getCookie("sessionId");
        auto username = req.getCookie("username");

        resp->setStatusCode(HttpResponse::HttpStatusCode::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/plain");

        resp->setBody("sessionId=" + sid.value_or("null") + "\nusername=" + username.value_or("null") + "\n");
    });

    server.setNumThreads(4);
    server.start();

    loop.loop();
}