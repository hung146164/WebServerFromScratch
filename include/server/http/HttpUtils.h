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
#include <sstream>
#include <iomanip>
#include <ctime>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "server/http/HttpRequest.h"
#include "server/http/HttpResponse.h"

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

    inline bool IsFormData(const HttpRequest &req)
    {
        return req.content_type.find("application/x-www-form-urlencoded") != std::string_view::npos;
    }

    inline bool IsMultipart(const HttpRequest &req)
    {
        return req.content_type.find("multipart/form-data") != std::string_view::npos;
    }

    inline void HandleFileUpload(int fd, const HttpRequest &req)
    {
        std::string upload_dir = "www/uploads";
        std::error_code ec;
        std::filesystem::create_directories(upload_dir, ec);

        std::string filename = "uploaded_file.bin";
        std::string_view file_data = req.body;

        std::string_view ct = req.content_type;
        auto boundary_pos = ct.find("boundary=");
        if (boundary_pos != std::string_view::npos)
        {
            std::string boundary = "--" + std::string(ct.substr(boundary_pos + 9));
            std::string_view body = req.body;

            auto start_pos = body.find(boundary);
            if (start_pos != std::string_view::npos)
            {
                size_t part_start = start_pos + boundary.size();
                auto end_pos = body.find(boundary, part_start);
                if (end_pos != std::string_view::npos)
                {
                    std::string_view part = body.substr(part_start, end_pos - part_start);

                    auto header_end = part.find("\r\n\r\n");
                    if (header_end == std::string_view::npos)
                    {
                        header_end = part.find("\n\n");
                    }

                    if (header_end != std::string_view::npos)
                    {
                        std::string_view headers = part.substr(0, header_end);
                        auto fn_pos = headers.find("filename=\"");
                        if (fn_pos != std::string_view::npos)
                        {
                            auto fn_end = headers.find("\"", fn_pos + 10);
                            if (fn_end != std::string_view::npos)
                            {
                                filename = std::string(headers.substr(fn_pos + 10, fn_end - (fn_pos + 10)));
                            }
                        }

                        size_t data_start = header_end + (part.find("\r\n\r\n") != std::string_view::npos ? 4 : 2);
                        file_data = part.substr(data_start);
                        if (!file_data.empty() && file_data.back() == '\n')
                        {
                            file_data.remove_suffix(1);
                            if (!file_data.empty() && file_data.back() == '\r')
                            {
                                file_data.remove_suffix(1);
                            }
                        }
                    }
                }
            }
        }
        else
        {
            std::string_view req_filename = GetHeader(req, "x-file-name");
            if (!req_filename.empty())
            {
                filename = std::string(req_filename);
            }
        }

        auto last_slash = filename.find_last_of("/\\");
        if (last_slash != std::string::npos)
        {
            filename = filename.substr(last_slash + 1);
        }

        std::string full_path = upload_dir + "/" + filename;
        std::ofstream out(full_path, std::ios::binary);
        if (!out.is_open())
        {
            Http::JSON(fd, 500, "{\"error\":\"Failed to save uploaded file\"}");
            return;
        }

        out.write(file_data.data(), file_data.size());
        out.close();

        std::string resp_json = "{\"message\":\"File uploaded successfully\",\"filename\":\"" + filename + "\",\"size\":" + std::to_string(file_data.size()) + "}";
        Http::JSON(fd, 201, resp_json);
    }

    inline std::string UrlDecode(std::string_view src)
    {
        std::string ret;
        ret.reserve(src.size());
        for (size_t i = 0; i < src.size(); ++i)
        {
            if (src[i] == '%' && i + 2 < src.size())
            {
                int value = 0;
                std::istringstream is(std::string(src.substr(i + 1, 2)));
                if (is >> std::hex >> value)
                {
                    ret += static_cast<char>(value);
                    i += 2;
                }
                else
                {
                    ret += src[i];
                }
            }
            else if (src[i] == '+')
            {
                ret += ' ';
            }
            else
            {
                ret += src[i];
            }
        }
        return ret;
    }

    inline void RC4(std::string_view key, std::string &data)
    {
        unsigned char S[256];
        for (int i = 0; i < 256; ++i)
            S[i] = i;

        int j = 0;
        for (int i = 0; i < 256; ++i)
        {
            j = (j + S[i] + static_cast<unsigned char>(key[i % key.size()])) % 256;
            std::swap(S[i], S[j]);
        }

        int i = 0;
        j = 0;
        for (size_t k = 0; k < data.size(); ++k)
        {
            i = (i + 1) % 256;
            j = (j + S[i]) % 256;
            std::swap(S[i], S[j]);
            unsigned char K = S[(S[i] + S[j]) % 256];
            data[k] ^= K;
        }
    }

    inline void LogRequest(int worker_id, std::string_view client_ip, const HttpRequest &req, int status_code)
    {
        time_t now = time(nullptr);
        char time_str[64];
        struct tm *tm_info = std::localtime(&now);
        if (tm_info)
        {
            std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        }
        else
        {
            std::strncpy(time_str, "UNKNOWN_TIME", sizeof(time_str));
        }

        std::string status_msg = Http::StatusMessage(status_code);
        std::string method_str = "UNKNOWN";
        switch (req.method)
        {
        case HttpMethod::GET:
            method_str = "GET";
            break;
        case HttpMethod::POST:
            method_str = "POST";
            break;
        case HttpMethod::PUT:
            method_str = "PUT";
            break;
        case HttpMethod::DELETE:
            method_str = "DELETE";
            break;
        case HttpMethod::HEAD:
            method_str = "HEAD";
            break;
        case HttpMethod::OPTIONS:
            method_str = "OPTIONS";
            break;
        default:
            method_str = "UNKNOWN";
            break;
        }

        std::cout << "[INFO] [" << time_str << "] [Worker " << worker_id << "] "
                  << method_str << " " << req.http_url << " -> "
                  << status_code << " " << status_msg << " (IP: " << client_ip << ")\n";
    }

} // namespace Http

#endif