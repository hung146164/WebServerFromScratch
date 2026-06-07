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
#include "server/http/HttpRequest.h"

namespace Http
{
    /// Lấy tham số từ Query String trong URL
    /// Dùng khi: GET request cần lọc/tìm kiếm/phân trang dữ liệu
    ///
    /// URL: /api/students?class=12A&page=2&limit=10
    /// Http::QueryParam(req, "class")  -> "12A"
    /// Http::QueryParam(req, "page")   -> "2"
    /// Http::QueryParam(req, "limit")  -> "10"
    ///
    /// Tại sao cần: Không có hàm này, người dùng phải tự cắt chuỗi
    /// URL thủ công rất phức tạp và dễ lỗi
    std::string_view QueryParam(const HttpRequest &req, std::string_view key);

    /// Lấy giá trị của một HTTP Header bất kỳ (không phân biệt hoa/thường)
    /// Dùng khi: Đọc Authorization token, Accept-Language, User-Agent,...
    ///
    /// Http::GetHeader(req, "authorization") -> "Bearer eyJhbGci..."
    /// Http::GetHeader(req, "accept-language") -> "vi-VN,vi;q=0.9"
    ///
    /// Tại sao cần: req.header dùng string_view làm key nên không thể
    /// tự động so sánh không phân biệt hoa/thường, dễ bị miss
    std::string_view GetHeader(const HttpRequest &req, std::string_view key);

    /// Lấy giá trị Cookie theo tên
    /// Dùng khi: Kiểm tra session đăng nhập, theme người dùng chọn,...
    ///
    /// Header gửi lên: Cookie: session_id=abc123; theme=dark
    /// Http::GetCookie(req, "session_id") -> "abc123"
    /// Http::GetCookie(req, "theme")      -> "dark"
    ///
    /// Tại sao cần: Tất cả cookie nằm trong 1 chuỗi dài phân cách bởi ";"
    /// phải tự tay cắt chuỗi rất phức tạp
    std::string_view GetCookie(const HttpRequest &req, std::string_view name);

    /// Kiểm tra nhanh Content-Type của request
    /// Dùng khi: Đầu mỗi handler có nhận body để xác nhận định dạng đúng
    /// trước khi parse, tránh crash khi parse sai định dạng
    ///
    /// if (!Http::IsJson(req)) { Http::Error(fd, 415, "..."); return; }
    bool IsJson(const HttpRequest &req);
    bool IsFormData(const HttpRequest &req);  // application/x-www-form-urlencoded
    bool IsMultipart(const HttpRequest &req); // multipart/form-data (upload file)

} // namespace Http

#endif