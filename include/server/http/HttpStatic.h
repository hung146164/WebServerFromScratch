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
#include "server/http/HttpResponse.h"

namespace Http
{

    /// Tự động nhận biết Content-Type từ đuôi file
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

    /// Phục vụ file tĩnh từ disk
    /// \param fd       Socket file descriptor của client
    /// \param url_path URL path từ HTTP request (ví dụ: "/index.html")
    /// \param web_root Thư mục gốc chứa file tĩnh (ví dụ: "./www")
    ///
    /// Tự động xử lý:
    ///   - / → index.html
    ///   - Path traversal attack (/../../../etc/passwd → 403)
    ///   - File không tồn tại → 404
    ///   - MIME type tự động theo đuôi file
    ///
    /// Ví dụ sử dụng:
    ///   Http::ServeFile(fd, req.http_url, "./www");
    inline void ServeFile(int fd, std::string_view url_path, std::string_view web_root)
    {
        // 1. Bỏ qua query string (?key=value)
        auto qpos = url_path.find('?');
        if (qpos != std::string_view::npos)
            url_path = url_path.substr(0, qpos);

        // 2. Mặc định URL "/" → index.html
        std::string rel_path(url_path);
        if (rel_path.empty() || rel_path == "/")
            rel_path = "/index.html";

        // 3. Xây dựng đường dẫn đầy đủ
        std::string full_path = std::string(web_root) + rel_path;

        // 4. Canonical hóa đường dẫn để phát hiện Path Traversal
        std::error_code ec;
        auto canonical_file = std::filesystem::weakly_canonical(full_path, ec);
        auto canonical_root = std::filesystem::weakly_canonical(std::string(web_root), ec);

        std::string file_str = canonical_file.string();
        std::string root_str = canonical_root.string();

        // 5. Kiểm tra bảo mật: file phải nằm trong web_root
        if (file_str.size() < root_str.size() ||
            file_str.substr(0, root_str.size()) != root_str)
        {
            Error(fd, 403, "Forbidden");
            return;
        }

        // 6. Mở file
        std::ifstream file(canonical_file, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            Error(fd, 404, "Not Found");
            return;
        }

        // 7. Đọc toàn bộ nội dung file vào bộ nhớ
        auto file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string content(static_cast<int>(file_size), '\0');
        file.read(content.data(), file_size);

        // 8. Gửi về client với MIME type đúng
        SendRaw(fd, 200, MimeType(file_str), content);
    }

} // namespace Http

#endif