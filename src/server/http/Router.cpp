/*!
    \file Router.cpp
    \brief Router implementation with path-only matching and wildcard fallback
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/

#include "server/http/Router.h"
#include <sys/socket.h>
#include <iostream>
#include <sstream>

std::unordered_map<std::string, HandlerFunc> Http::Router::routes;
HandlerFunc Http::Router::fallback_handler = nullptr;

std::string Http::Router::MethodToString(HttpMethod method)
{
    switch (method)
    {
    case HttpMethod::GET:
        return "GET";
    case HttpMethod::POST:
        return "POST";
    case HttpMethod::PUT:
        return "PUT";
    case HttpMethod::DELETE:
        return "DELETE";
    case HttpMethod::HEAD:
        return "HEAD";
    case HttpMethod::OPTIONS:
        return "OPTIONS";
    default:
        return "UNKNOWN";
    }
}

// Tach path ra khoi URL (bo query string ?key=val)
static std::string ExtractPath(std::string_view url)
{
    auto qpos = url.find('?');
    if (qpos != std::string_view::npos)
        return std::string(url.substr(0, qpos));
    return std::string(url);
}

void Http::Router::Register(HttpMethod method, const std::string &path, HandlerFunc handler)
{
    std::string key = MethodToString(method) + ":" + path;
    routes[key] = handler;
    std::cout << "[Router] Registered: " << key << "\n";
}

void Http::Router::RegisterFallback(HandlerFunc handler)
{
    fallback_handler = handler;
    std::cout << "[Router] Registered fallback handler\n";
}

void Http::Router::Dispatch(int fd, const HttpRequest &req)
{
    // Xu ly OPTIONS preflight CORS
    if (req.method == HttpMethod::OPTIONS)
    {
        std::string res =
            "HTTP/1.1 200 OK\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
            "Content-Length: 0\r\n\r\n";
        send(fd, res.data(), res.size(), MSG_NOSIGNAL);
        return;
    }

    // Chi dung PATH (bo query string) lam key tim kiem
    std::string path = ExtractPath(req.http_url);
    std::string key = MethodToString(req.method) + ":" + path;

    auto it = routes.find(key);
    if (it != routes.end())
    {
        it->second(fd, req);
        return;
    }

    // Fallback handler (dung cho static file serving)
    if (fallback_handler)
    {
        fallback_handler(fd, req);
        return;
    }

    // 404
    SendErrorResponse(fd, 404, "Not Found", "{\"error\":\"Route Not Found\"}");
}

void Http::Router::SendErrorResponse(int fd, int status_code,
                                     const std::string &status_msg,
                                     const std::string &body)
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_msg << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Connection: keep-alive\r\n\r\n"
        << body;
    std::string response = oss.str();
    send(fd, response.data(), response.size(), MSG_NOSIGNAL);
}
