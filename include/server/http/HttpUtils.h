/*!
    \file HttpUtils.h
    \brief Helper functions for reading HTTP request data
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_HTTP_HTTPUTILS_H
#define CPPSERVER_HTTP_HTTPUTILS_H

#include <string_view>
#include <string>
#include <cctype>
#include "server/http/HttpRequest.h"

namespace Http
{

    /// Lấy tham số Query String từ URL
    /// URL: /api/students?class=12A&page=2
    /// Http::QueryParam(req, "class") → "12A"
    inline std::string_view QueryParam(const HttpRequest &req, std::string_view key)
    {
        std::string_view url = req.http_url;
        auto qpos = url.find('?');
        if (qpos == std::string_view::npos)
            return {};

        std::string_view query = url.substr(qpos + 1);
        int start = 0;

        while (start < query.size())
        {
            int amp = query.find('&', start);
            std::string_view pair = (amp == std::string_view::npos)
                                        ? query.substr(start)
                                        : query.substr(start, amp - start);

            int eq = pair.find('=');
            if (eq != std::string_view::npos && pair.substr(0, eq) == key)
                return pair.substr(eq + 1);

            if (amp == std::string_view::npos)
                break;
            start = amp + 1;
        }
        return {};
    }

    /// Lấy giá trị Header theo tên (không phân biệt hoa/thường)
    /// Http::GetHeader(req, "authorization") → "Bearer xyz"
    inline std::string_view GetHeader(const HttpRequest &req, std::string_view key)
    {
        for (const auto &[k, v] : req.header)
        {
            if (k.size() != key.size())
                continue;
            bool match = true;
            for (int i = 0; i < key.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(k[i])) !=
                    std::tolower(static_cast<unsigned char>(key[i])))
                {
                    match = false;
                    break;
                }
            }
            if (match)
                return v;
        }
        return {};
    }

    /// Lấy giá trị Cookie theo tên
    /// Cookie header: "session=abc; theme=dark"
    /// Http::GetCookie(req, "session") → "abc"
    inline std::string_view GetCookie(const HttpRequest &req, std::string_view name)
    {
        std::string_view cookie_hdr = GetHeader(req, "cookie");
        if (cookie_hdr.empty())
            return {};

        int start = 0;
        while (start < cookie_hdr.size())
        {
            // Bỏ khoảng trắng
            while (start < cookie_hdr.size() && cookie_hdr[start] == ' ')
                ++start;

            int semi = cookie_hdr.find(';', start);
            std::string_view pair = (semi == std::string_view::npos)
                                        ? cookie_hdr.substr(start)
                                        : cookie_hdr.substr(start, semi - start);

            int eq = pair.find('=');
            if (eq != std::string_view::npos && pair.substr(0, eq) == name)
                return pair.substr(eq + 1);

            if (semi == std::string_view::npos)
                break;
            start = semi + 1;
        }
        return {};
    }

    /// Lấy URL path không có query string
    /// "/api/student?page=1" → "/api/student"
    inline std::string_view GetPath(const HttpRequest &req)
    {
        std::string_view url = req.http_url;
        auto qpos = url.find('?');
        return (qpos == std::string_view::npos) ? url : url.substr(0, qpos);
    }

    // ─── Kiểm tra Content-Type ────────────────────────────────────────────────────

    inline bool IsJson(const HttpRequest &req)
    {
        return req.content_type.find("application/json") != std::string_view::npos;
    }

    inline bool IsFormData(const HttpRequest &req)
    {
        return req.content_type.find("application/x-www-form-urlencoded") != std::string_view::npos;
    }

    inline bool IsMultipart(const HttpRequest &req)
    {
        return req.content_type.find("multipart/form-data") != std::string_view::npos;
    }

} // namespace Http

#endif