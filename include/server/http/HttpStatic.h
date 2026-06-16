/*!
    \file HttpStatic.h
    \brief Static file serving with MIME detection and path traversal protection
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_HTTP_HTTPSTATIC_H
#define CPPSERVER_HTTP_HTTPSTATIC_H

#include <string>
#include <string_view>
#include <fstream>
#include <filesystem>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>

#include "server/http/HttpResponse.h"

namespace Http
{

    inline std::string_view MimeType(std::string_view path)
    {
        auto pos = path.rfind('.');
        if (pos == std::string_view::npos)
            return "application/octet-stream";

        std::string_view ext = path.substr(pos);

        if (ext == ".html" || ext == ".htm")
            return "text/html; charset=utf-8";
        if (ext == ".css")
            return "text/css; charset=utf-8";
        if (ext == ".js")
            return "application/javascript; charset=utf-8";
        if (ext == ".json")
            return "application/json; charset=utf-8";
        if (ext == ".xml")
            return "application/xml; charset=utf-8";
        if (ext == ".txt")
            return "text/plain; charset=utf-8";
        if (ext == ".png")
            return "image/png";
        if (ext == ".jpg" || ext == ".jpeg")
            return "image/jpeg";
        if (ext == ".gif")
            return "image/gif";
        if (ext == ".svg")
            return "image/svg+xml";
        if (ext == ".ico")
            return "image/x-icon";
        if (ext == ".webp")
            return "image/webp";
        if (ext == ".woff")
            return "font/woff";
        if (ext == ".woff2")
            return "font/woff2";
        if (ext == ".ttf")
            return "font/ttf";
        if (ext == ".pdf")
            return "application/pdf";
        if (ext == ".zip")
            return "application/zip";
        return "application/octet-stream";
    }

    inline void ServeFile(int fd, const HttpRequest &req, std::string_view web_root, std::string_view req_range = "")
    {
        std::string_view url_path = req.http_url;
        bool force_download = false;
        auto qpos = url_path.find('?');
        if (qpos != std::string_view::npos)
        {
            std::string_view query = url_path.substr(qpos + 1);

            if (query.find("download=true") != std::string_view::npos)
            {
                force_download = true;
            }

            url_path = url_path.substr(0, qpos);
        }

        // 2. Mặc định URL "/" → index.html
        std::string rel_path(url_path);
        if (rel_path.empty() || rel_path == "/")
            rel_path = "/index.html";

        // 3. Xây dựng đường dẫn đầy đủ
        std::string full_path = std::string(web_root) + rel_path;

        // 4. Canonical hóa đường dẫn (Chống leo rào)
        std::error_code ec;
        auto canonical_file = std::filesystem::weakly_canonical(full_path, ec);
        auto canonical_root = std::filesystem::weakly_canonical(std::string(web_root), ec);

        std::string file_str = canonical_file.string();
        std::string root_str = canonical_root.string();

        // 5. Kiểm tra bảo mật
        if (file_str.size() < root_str.size() || file_str.substr(0, root_str.size()) != root_str)
        {
            Error(fd, 403, "Forbidden");
            return;
        }

        // 6. Lấy kích thước file
        struct stat file_stat{};
        if (stat(file_str.c_str(), &file_stat) < 0)
        {
            Error(fd, 404, "Not Found");
            return;
        }
        off_t file_size = file_stat.st_size;

        // Caching: Tạo tiêu đề Last-Modified dạng GMT
        char last_mod_str[64];
        struct tm *tm_info = std::gmtime(&file_stat.st_mtime);
        std::strftime(last_mod_str, sizeof(last_mod_str), "%a, %d %b %Y %H:%M:%S GMT", tm_info);

        // Kiểm tra If-Modified-Since từ client
        std::string_view if_mod_since = "";
        auto it_mod = req.header.find("If-Modified-Since");
        if (it_mod != req.header.end())
        {
            if_mod_since = it_mod->second;
        }

        if (!if_mod_since.empty() && if_mod_since == last_mod_str)
        {
            // File không đổi, trả về 304 rỗng để tiết kiệm băng thông mạng
            std::string res = "HTTP/1.1 304 Not Modified\r\n"
                              "Last-Modified: " +
                              std::string(last_mod_str) + "\r\n"
                                                          "Connection: keep-alive\r\n\r\n";
            send(fd, res.data(), res.size(), MSG_NOSIGNAL);
            return;
        }

        // 7. Mở file
        int file_fd = open(file_str.c_str(), O_RDONLY);
        if (file_fd < 0)
        {
            Error(fd, 404, "Not Found");
            return;
        }

        // ---- Trích xuất Tên file từ đường dẫn tương đối ----
        std::string_view filename = rel_path;
        auto slash_pos = filename.rfind('/');
        if (slash_pos != std::string_view::npos)
        {
            filename = filename.substr(slash_pos + 1);
        }

        // ---- Xử lý Tải Tiếp (IDM / Pause & Resume) ----
        off_t offset = 0;
        off_t send_size = file_size;
        bool is_partial = false;

        // Nếu Client xin tải tiếp từ một vị trí (ví dụ: bytes=1048576-)
        if (!req_range.empty() && req_range.find("bytes=") == 0)
        {
            auto dash_pos = req_range.find('-');
            if (dash_pos != std::string_view::npos)
            {
                std::string start_str = std::string(req_range.substr(6, dash_pos - 6));
                std::string end_str = std::string(req_range.substr(dash_pos + 1));

                off_t start_byte = start_str.empty() ? 0 : std::stoll(start_str);
                off_t end_byte = end_str.empty() ? file_size - 1 : std::stoll(end_str);

                if (start_byte < file_size)
                {
                    if (end_byte >= file_size)
                        end_byte = file_size - 1;
                    offset = start_byte;
                    send_size = end_byte - start_byte + 1;
                    is_partial = true;
                }
            }
        }

        // 8. Ráp Header gửi về
        std::string header;
        header.reserve(512);

        if (is_partial)
        {
            header += "HTTP/1.1 206 Partial Content\r\n";
            header += "Content-Range: bytes " + std::to_string(offset) + "-" +
                      std::to_string(offset + send_size - 1) + "/" + std::to_string(file_size) + "\r\n";
        }
        else
        {
            header += "HTTP/1.1 200 OK\r\n";
            header += "Accept-Ranges: bytes\r\n"; // Báo cho IDM biết là hỗ trợ tải tiếp
        }

        header += "Content-Type: ";
        header += MimeType(rel_path); // FIX: Dùng rel_path thay vì file_str để tránh sai đuôi mở rộng khi root path có dấu "."
        header += "\r\nContent-Length: ";
        header += std::to_string(send_size);
        header += "\r\n";

        // ---- Ép trình duyệt "Save As" ----
        if (force_download)
        {
            header += "Content-Disposition: attachment; filename=\"";
            header += filename;
            header += "\"\r\n";
        }
        header += "Last-Modified: " + std::string(last_mod_str) + "\r\n";
        header += "Access-Control-Allow-Origin: *\r\nConnection: keep-alive\r\n\r\n";
        send(fd, header.data(), header.size(), MSG_NOSIGNAL);

        // 9. Vòng lặp bơm dữ liệu Zero-Copy
        ssize_t remaining = (ssize_t)send_size;
        int timeout_count = 0; // Đếm số lần bị nghẽn mạng liên tiếp

        while (remaining > 0)
        {
            ssize_t sent = sendfile(fd, file_fd, &offset, (size_t)remaining);
            if (sent > 0)
            {
                remaining -= sent;
                timeout_count = 0; // Gửi thành công thì xóa đếm nghẽn mạng
            }
            else if (sent == 0)
            {
                break; // Client chủ động đóng kết nối
            }
            else // sent < 0
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    fd_set wfds;
                    FD_ZERO(&wfds);
                    FD_SET(fd, &wfds);

                    struct timeval tv{0, 500000}; // Chờ tối đa 0.5 giây
                    int sel = select(fd + 1, nullptr, &wfds, nullptr, &tv);

                    if (sel < 0)
                        break; // Lỗi cấu trúc socket
                    if (sel == 0)
                    {
                        timeout_count++;
                        if (timeout_count > 60)
                            break; // Quá 30 giây kẹt liên tục mới ngắt kết nối
                        continue;
                    }
                    continue; // Ống thông thì quay lên gửi tiếp
                }
                break; // Lỗi đứt kết nối (EPIPE, ECONNRESET...)
            }
        }
        close(file_fd);
    }

} // namespace Http

#endif