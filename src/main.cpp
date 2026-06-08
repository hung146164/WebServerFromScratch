/*!
    \file main.cpp
    \brief Demo - English Center Management with JSON file persistence
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/

#include "network/NetworkServer.h"
#include "network/ServerConfig.h"
#include "server/http/Router.h"
#include "server/http/HttpResponse.h"
#include "server/http/HttpStatic.h"
#include "server/http/HttpUtils.h"
#include "server/http/HttpMethod.h"

#include <fstream>
#include <sstream>
#include <mutex>
#include <filesystem>
#include <cstdio>

// ─── main ────────────────────────────────────────────────────────────────────
signed main()
{
    Http::Router::Register(HttpMethod::GET, "/api/ping",
                           [](int fd, const HttpRequest &req)
                           {
                               Http::JSON(fd, 200, R"({"status":"pong","server":"WebServerVdt"})");
                           });
    Http::Router::Register(HttpMethod::GET, "/api/download",
                           [](int fd, const HttpRequest &req)
                           {
                               Http::ServeFile(fd, "/test-download-1.5GB.bin", "./www");
                           });
    Http::Router::RegisterFallback(
        [](int fd, const HttpRequest &req)
        {
            Http::ServeFile(fd, req.http_url, "./www");
        });

    ServerConfig cfg;
    cfg.port = 8081;
    cfg.num_workers = 2;
    cfg.client_per_worker = 200;
    cfg.max_epoll_events = 512;

    NetworkServer server(cfg);
    server.Start();
    return 0;
}
