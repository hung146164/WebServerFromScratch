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

    inline void HandleFileUpload(int fd, const HttpRequest &req)
    {
        /*
            Tạo đường dẫn thư mục đích là "www/upload"
            Gọi thư viện hệ thống để tự động tạo toàn bộ cây thư mục nếu nó chưa tồn tại
        */
        std::string upload_dir = "www/upload";
        std::error_code ec;
        std::filesystem::create_directories(upload_dir, ec);

        /*
            Khởi tạo
        */
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

        // Validate file extension matches what the server supports
        auto IsSupportedExtension = [](std::string_view fn) -> bool
        {
            auto pos = fn.rfind('.');
            if (pos == std::string_view::npos)
                return false;
            std::string ext;
            for (size_t i = pos; i < fn.size(); ++i)
            {
                ext += (char)std::tolower((unsigned char)fn[i]);
            }
            return (ext == ".html" || ext == ".htm" || ext == ".css" || ext == ".js" ||
                    ext == ".json" || ext == ".xml" || ext == ".txt" || ext == ".png" ||
                    ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".svg" ||
                    ext == ".ico" || ext == ".webp" || ext == ".woff" || ext == ".woff2" ||
                    ext == ".ttf" || ext == ".pdf" || ext == ".zip");
        };

        if (!IsSupportedExtension(filename))
        {
            Http::JSON(fd, 400, "{\"error\":\"File format not supported. Only web assets (.html, .css, .js, .png, .pdf, .zip, etc.) are allowed.\"}");
            return;
        }

        std::string unique_filename = std::to_string(std::time(nullptr)) + "_" + std::to_string(fd) + "_" + filename;
        std::string full_path = upload_dir + "/" + unique_filename;
        std::ofstream out(full_path, std::ios::binary);
        if (!out.is_open())
        {
            Http::JSON(fd, 500, "{\"error\":\"Failed to save uploaded file\"}");
            return;
        }

        out.write(file_data.data(), file_data.size());
        out.close();

        std::string resp_json = "{\"message\":\"File uploaded successfully\",\"filename\":\"" + unique_filename + "\",\"size\":" + std::to_string(file_data.size()) + "}";
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

    inline void LogRequest(int worker_id, std::string_view client_ip, const HttpRequest &req, int status_code)
    {
        time_t now = time(nullptr);
        char time_str[64];
        struct tm tm_res;
        struct tm *tm_info = localtime_r(&now, &tm_res);
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

        // Construct the log line as a single string to print atomically to std::cout,
        // avoiding interleaved logs from concurrent worker threads.
        std::string log_line = "[INFO] [" + std::string(time_str) + "] [Worker " + std::to_string(worker_id) + "] " + method_str + " " + std::string(req.http_url) + " -> " + std::to_string(status_code) + " " + status_msg + " (IP: " + std::string(client_ip) + ")\n";
        std::cout << log_line;
    }

} // namespace Http

#endif