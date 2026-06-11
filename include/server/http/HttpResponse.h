/*!
    \file HttpResponse.h
    \brief Helper functions for sending HTTP responses
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_HTTP_HTTPRESPONSE_H
#define CPPSERVER_HTTP_HTTPRESPONSE_H

#include <string>
#include <string_view>
#include <sstream>
#include <sys/socket.h>

namespace Http
{

    // ─── Nội bộ ──────────────────────────────────────────────────────────────────

    inline std::string StatusMessage(int code)
    {
        switch (code)
        {
        case 200:
            return "OK";
        case 201:
            return "Created";
        case 204:
            return "No Content";
        case 301:
            return "Moved Permanently";
        case 302:
            return "Found";
        case 304:
            return "Not Modified";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 409:
            return "Conflict";
        case 413:
            return "Payload Too Large";
        case 415:
            return "Unsupported Media Type";
        case 429:
            return "Too Many Requests";
        case 500:
            return "internal Server Error";
        default:
            return "Unknown";
        }
    }

    inline void SendRaw(int fd, int status,
                        std::string_view content_type,
                        std::string_view body)
    {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << status << " " << StatusMessage(status) << "\r\n"
            << "Content-Type: " << content_type << "\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Access-Control-Allow-Origin: *\r\n"
            << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
            << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
            << "Connection: keep-alive\r\n\r\n";

        std::string header = oss.str();

        // Gửi header
        send(fd, header.data(), header.size(), MSG_NOSIGNAL);
        // Gửi body (nếu có)
        if (!body.empty())
            send(fd, body.data(), body.size(), MSG_NOSIGNAL);
    }

    // ─── API công khai ────────────────────────────────────────────────────────────

    inline void JSON(int fd, int status, std::string_view body)
    {
        SendRaw(fd, status, "application/json; charset=utf-8", body);
    }

    inline void HTML(int fd, int status, std::string_view body)
    {
        SendRaw(fd, status, "text/html; charset=utf-8", body);
    }

    inline void Text(int fd, int status, std::string_view body)
    {
        SendRaw(fd, status, "text/plain; charset=utf-8", body);
    }

    inline void Redirect(int fd, std::string_view location)
    {
        std::ostringstream oss;
        oss << "HTTP/1.1 302 Found\r\n"
            << "Location: " << location << "\r\n"
            << "Content-Length: 0\r\n"
            << "Connection: keep-alive\r\n\r\n";
        std::string res = oss.str();
        send(fd, res.data(), res.size(), MSG_NOSIGNAL);
    }

    /// Http::Error(fd, 404, "Not Found");
    inline void Error(int fd, int status, std::string_view message)
    {
        std::ostringstream oss;
        oss << "{\"error\":\"" << message << "\",\"status\":" << status << "}";
        JSON(fd, status, oss.str());
    }

    /// Http::NoContent(fd);  — dùng sau DELETE thành công
    inline void NoContent(int fd)
    {
        std::string res = "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
        send(fd, res.data(), res.size(), MSG_NOSIGNAL);
    }

    /// Xử lý OPTIONS preflight của CORS (trình duyệt gửi trước POST/PUT)
    inline void HandleCORSPreflight(int fd)
    {
        std::string res =
            "HTTP/1.1 200 OK\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
            "Access-Control-Max-Age: 86400\r\n"
            "Content-Length: 0\r\n\r\n";
        send(fd, res.data(), res.size(), MSG_NOSIGNAL);
    }

} // namespace Http

#endif