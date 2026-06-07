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
    // ─── Nội bộ, người dùng không gọi trực tiếp ──────────────────────────────

    /// Chuyển mã status HTTP thành chuỗi mô tả
    /// Cần thiết vì send() phải gửi chuỗi "200 OK", "404 Not Found"...
    /// chứ không phải con số thuần túy
    inline std::string StatusMessage(int code);

    /// Hàm gửi response lõi, tất cả hàm bên dưới đều gọi vào đây
    /// Tự động ghép HTTP headers + body thành đúng định dạng HTTP/1.1
    /// Tự động thêm CORS header để Frontend không bị trình duyệt chặn
    inline void SendRaw(int fd, int status,
                        std::string_view content_type,
                        std::string_view body);

    // ─── API công khai cho người dùng Framework ───────────────────────────────

    /// Gửi JSON response
    /// Dùng khi: Hầu hết mọi API endpoint trả dữ liệu về cho Frontend
    /// Ví dụ: Http::JSON(fd, 200, "{\"name\":\"Nam\"}");
    inline void JSON(int fd, int status, std::string_view body);

    /// Gửi HTML response
    /// Dùng khi: Render trang web trực tiếp từ Server (Server-Side Rendering)
    /// Ví dụ: Http::HTML(fd, 200, "<h1>Xin chào</h1>");
    inline void HTML(int fd, int status, std::string_view body);

    /// Gửi plain text response
    /// Dùng khi: Debug, health check endpoint, webhook đơn giản
    /// Ví dụ: Http::Text(fd, 200, "pong");
    inline void Text(int fd, int status, std::string_view body);

    /// Chuyển hướng client sang URL khác (302 redirect)
    /// Dùng khi: Người dùng chưa đăng nhập -> chuyển sang /login
    ///           Sau khi submit form -> chuyển sang trang kết quả
    /// Ví dụ: Http::Redirect(fd, "/login");
    inline void Redirect(int fd, std::string_view location);

    /// Gửi response lỗi dạng JSON chuẩn hóa
    /// Dùng khi: Bất kỳ lỗi nào trong handler (validate thất bại, không tìm thấy,...)
    /// Ví dụ: Http::Error(fd, 403, "You don't have permission");
    inline void Error(int fd, int status, std::string_view message);

} // namespace Http

#endif