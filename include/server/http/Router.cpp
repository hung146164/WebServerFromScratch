/*!
    \file Router.cpp
    \brief Router implementation
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/

#include "server/http/Router.h"
#include <sys/socket.h>
#include <iostream>
#include <sstream>

namespace Http
{

    // Khởi tạo map static lưu trữ routes
    std::unordered_map<std::string, HandlerFunc> Router::routes;

    std::string Router::MethodToString(HttpMethod method)
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

    void Router::Register(HttpMethod method, const std::string &path, HandlerFunc handler)
    {
        std::string key = MethodToString(method) + ":" + path;
        routes[key] = handler;
        std::cout << "[Router] Đã đăng ký route: " << key << "\n";
    }

    void Router::Dispatch(int fd, const HttpRequest &req)
    {
        // Tạo key tìm kiếm từ Method và URL của Request (chuyển http_url từ string_view sang string)
        std::string key = MethodToString(req.method) + ":" + std::string(req.http_url);

        auto it = routes.find(key);
        if (it != routes.end())
        {
            // Tìm thấy Route -> Gọi Handler tương ứng
            it->second(fd, req);
        }
        else
        {
            // Không tìm thấy Route -> Trả về 404 Not Found
            SendErrorResponse(fd, 404, "Not Found", "{\"error\":\"Route Not Found\"}");
        }
    }

    void Router::SendErrorResponse(int fd, int status_code, const std::string &status_msg, const std::string &body)
    {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << status_code << " " << status_msg << "\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n\r\n"
            << body;

        std::string response = oss.str();
        send(fd, response.data(), response.size(), 0);
    }

} // namespace Http