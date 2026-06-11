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

void test_download_image(int fd, const HttpRequest &request)
{
    for (auto &i : request.header)
    {
        std::cout << i.first << ' ' << i.second << '\n';
    }
    std::string_view req_range = "";
    auto it = request.header.find("Range");
    if (it != request.header.end())
    {
        req_range = it->second;
    }
    Http::ServeFile(fd, request.http_url, "www", req_range);
}
void fallback_test(int fd, const HttpRequest &request)
{

    Http::ServeFile(fd, request.http_url, "www");
}
int main()
{
    ServerConfig cfg;
    cfg.port = 8081;
    cfg.num_workers = 2;

    Http::Router::Register(HttpMethod::GET, "/api/download_image/testfile.img", test_download_image);
    Http::Router::Register(HttpMethod::GET, "/api/download_image/de1.jpg", test_download_image);
    Http::Router::Register(HttpMethod::GET, "/api/download_image/222.pdf", test_download_image);

    Http::Router::RegisterFallback(fallback_test);

    NetworkServer server(cfg);
    server.Start();
    return 0;
}
