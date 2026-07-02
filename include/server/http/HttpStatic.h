/*!
    \file HttpStatic.h
    \brief Helper functions for serving static files
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
#include "server/http/HttpUtils.h"

namespace Http
{

    inline std::string_view MimeType(std::string_view path)
    {
        /*
            Tìm dấu chấm '.' cuối cùng trong tên đường dẫn để xác định
            đuôi mở rộng của file. Nếu không thấy dấu chấm nào, trả về kiểu
            mặc định "application/octet-stream"
        */
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

    inline void ServeFile(int fd, const HttpRequest &req, std::string_view url_to_serve,
                          std::string_view web_root, bool inline_view)
    {
        std::string decoded_url = Http::UrlDecode(url_to_serve);
        std::string_view url_path = decoded_url;

        /*
            Nếu truy cập rỗng hoặc / thì mặc định là index.html
        */
        std::string rel_path(url_path);
        if (rel_path.empty() || rel_path == "/")
            rel_path = "/index.html";

        // Xây đường dẫn dầy đủ đến chỗ file
        std::string full_path = std::string(web_root) + rel_path;

        /*
        Hàm std::filesystem::weakly_canonical có nhiệm vụ chuẩn hóa đường dẫn
         (loại bỏ các ký tự thừa như ., .., hoặc các liên kết - symlinks)
         để đưa về một đường dẫn tuyệt đối duy nhất và "sạch sẽ" nhất,
         ngay cả khi file đó chưa tồn tại.
         để tránh hacker kiểu : ../../../../etc/passwd

        */
        std::error_code ec;
        auto canonical_file = std::filesystem::weakly_canonical(full_path, ec);
        auto canonical_root = std::filesystem::weakly_canonical(std::string(web_root), ec);
        std::string file_str = canonical_file.string();
        std::string root_str = canonical_root.string();

        if (!root_str.empty() && root_str.back() != '/' && root_str.back() != '\\')
        {
            root_str += '/';
        }

        /*
        Kiểm tra xem file yêu cầu có nằm trong thư mục gốc được phép phục vụ hay không.
        Nếu hacker dùng mẹo gửi ../../etc/passwd làm đường dẫn file nhảy ra ngoài thư mục gốc,
        hệ thống sẽ chặn đứng và trả về lỗi 403 Forbidden
        */
        if (file_str.size() < root_str.size() || file_str.substr(0, root_str.size()) != root_str)
        {
            Error(fd, 403, "Forbidden");
            return;
        }

        /*
            Kiểm tra xem file có thực sự tồn tại
        */
        struct stat file_stat{};
        if (stat(file_str.c_str(), &file_stat) < 0)
        {
            Error(fd, 404, "Not Found");
            return;
        }
        off_t file_size = file_stat.st_size;

        /*
            Mở file
        */
        int file_fd = open(file_str.c_str(), O_RDONLY);
        if (file_fd < 0)
        {
            Error(fd, 404, "Not Found");
            return;
        }

        std::string_view filename = rel_path;
        auto slash_pos = filename.rfind('/');
        if (slash_pos != std::string_view::npos)
        {
            filename = filename.substr(slash_pos + 1);
        }

        // Ráp Header gửi về
        std::string header;
        header.reserve(512);

        header += "HTTP/1.1 200 OK\r\n";

        header += "Content-Type: ";
        header += MimeType(rel_path);
        header += "\r\nContent-Length: ";
        header += std::to_string(file_size);
        header += "\r\n";

        // ---- Thiết lập Content-Disposition theo cờ inline_view ----
        // Trình duyệt sẽ biết nếu Content-Disposition: attachment nó sẽ tự động tải file
        if (inline_view)
        {
            header += "Content-Disposition: inline\r\n";
        }
        else
        {
            header += "Content-Disposition: attachment; filename=\"";
            header += filename;
            header += "\"\r\n";
        }

        header += "Access-Control-Allow-Origin: *\r\nConnection: keep-alive\r\n\r\n";
        send(fd, header.data(), header.size(), MSG_NOSIGNAL);

        // Thiết lập trạng thái gửi file ngầm, thông qua cờ epoll out của worker

        req.is_sending_file = true;
        req.file_fd = file_fd;
        req.file_offset = 0;
        req.file_remaining = file_size;
        req.last_speed_check_time = time(nullptr);
        req.bytes_sent_in_period = 0;
    }

} // namespace Http

#endif