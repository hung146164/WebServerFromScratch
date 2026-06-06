/*!
    \file Router.h
    \brief Router definition
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/

#ifndef CPPSERVER_HTTP_ROUTER_H
#define CPPSERVER_HTTP_ROUTER_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>

#include "server/http/HttpRequest.h"
#include "server/http/HttpMethod.h"

namespace Http
{

    // Định nghĩa kiểu hàm xử lý request: nhận socket fd và tham chiếu HttpRequest
    using HandlerFunc = std::function<void(int fd, const HttpRequest &req)>;

    class Router
    {
    public:
        // Đăng ký một Route mới
        static void Register(HttpMethod method, const std::string &path, HandlerFunc handler);

        // Điều hướng request đến Handler tương ứng
        static void Dispatch(int fd, const HttpRequest &req);

    private:
        // Helper để chuyển đổi HttpMethod enum thành chuỗi string dùng làm Key
        static std::string MethodToString(HttpMethod method);

        // Helper để gửi nhanh các phản hồi lỗi HTTP cơ bản
        static void SendErrorResponse(int fd, int status_code, const std::string &status_msg, const std::string &body);

        // Map lưu trữ: Key là "METHOD:PATH" (Ví dụ: "POST:/api/student"), Value là hàm xử lý
        static std::unordered_map<std::string, HandlerFunc> routes;
    };

} // namespace Http

#endif // CPPSERVER_HTTP_ROUTER_H