#include "server/http/Router.h"
#include "server/http/HttpUtils.h"
#include <sys/socket.h>
#include <iostream>
#include <sstream>
#include <ctime>
#include <unordered_map>

std::unordered_map<std::string, HandlerFunc> Http::Router::routes;
HandlerFunc Http::Router::fallback_handler = nullptr;
int Http::Router::rate_limit_per_sec = 20;

// Khai báo biến thread-local của worker ID (được định nghĩa trong Worker.cpp)
extern thread_local int current_worker_id;

// Cấu trúc phục vụ cho bộ giới hạn tần suất yêu cầu (Rate Limiting)
struct RateLimitInfo
{
    int count = 0;
    time_t window_start = 0;
};

// Lưu thông tin giới hạn truy cập của từng IP trên mỗi Thread Worker (Không cần khóa Mutex!)
thread_local std::unordered_map<std::string, RateLimitInfo> ip_rate_limits;

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
    // --- 1. Giới Hạn Tần Suất Yêu Cầu (Rate Limiting) ---
    time_t now = time(nullptr);
    auto &limit_info = ip_rate_limits[std::string(req.client_ip)];
    if (now - limit_info.window_start >= 1)
    {
        limit_info.count = 1;
        limit_info.window_start = now;
    }
    else
    {
        limit_info.count++;
        if (limit_info.count > rate_limit_per_sec) // Giới hạn cấu hình động
        {
            SendErrorResponse(fd, 429, "Too Many Requests", "{\"error\":\"Rate limit exceeded. Max " + std::to_string(rate_limit_per_sec) + " req/sec.\"}");
            Http::LogRequest(current_worker_id, req.client_ip, req, 429);
            return;
        }
    }

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
        Http::LogRequest(current_worker_id, req.client_ip, req, 200);
        return;
    }

    // --- 2. Giải Mã URL (Percent-Decoding) ---
    std::string path = ExtractPath(req.http_url);
    path = Http::UrlDecode(path);
    std::string key = MethodToString(req.method) + ":" + path;

    // 1. Khớp tuyệt đối trước (Exact Match)
    auto it = routes.find(key);
    if (it != routes.end())
    {
        it->second(fd, req);
        Http::LogRequest(current_worker_id, req.client_ip, req, 200);
        return;
    }

    // 2. Khớp tiền tố (Wildcard / Prefix Match, ví dụ route path kết thúc bằng *)
    for (const auto &pair : routes)
    {
        const std::string &route_key = pair.first;
        if (!route_key.empty() && route_key.back() == '*')
        {
            std::string prefix = route_key.substr(0, route_key.size() - 1);
            if (key.size() >= prefix.size() && key.compare(0, prefix.size(), prefix) == 0)
            {
                pair.second(fd, req);
                Http::LogRequest(current_worker_id, req.client_ip, req, 200);
                return;
            }
        }
    }

    if (fallback_handler)
    {
        fallback_handler(fd, req);
        Http::LogRequest(current_worker_id, req.client_ip, req, 200);
        return;
    }

    // 404
    SendErrorResponse(fd, 404, "Not Found", "{\"error\":\"Route Not Found\"}");
    Http::LogRequest(current_worker_id, req.client_ip, req, 404);
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
