#include "network/NetworkServer.h"
#include "network/ServerConfig.h"
#include "server/http/Router.h"
#include "server/http/HttpResponse.h"
#include "server/http/HttpStatic.h"
#include "server/http/HttpUtils.h"
#include "server/http/HttpMethod.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <filesystem>
#include <cstdio>
#include <csignal>

NetworkServer *global_server = nullptr;

void signal_handler(int signal)
{
    if (global_server)
    {
        if (signal == SIGHUP)
        {
            global_server->ReloadConfig();
        }
        else
        {
            std::cout << "\n[Server] Signal (" << signal << ") received. Shutting down gracefully...\n";
            global_server->Stop();
        }
    }
}

void test_download_image(int fd, const HttpRequest &request)
{
    for (auto &i : request.header)
    {
        std::cout << i.first << ' ' << i.second << '\n';
    }
    std::string_view req_range = "";
    auto it = request.header.find("Range");
    if (it != request.header.end())
    {
        req_range = it->second;
    }
    Http::ServeFile(fd, request, "www", req_range);
}

void fallback_test(int fd, const HttpRequest &request)
{
    std::string_view req_range = "";
    auto it = request.header.find("Range");
    if (it != request.header.end())
    {
        req_range = it->second;
    }
    Http::ServeFile(fd, request, "www", req_range);
}

// Handler minh họa mã hóa Payload ở tầng ứng dụng bằng RC4 tự viết
void secure_data_handler(int fd, const HttpRequest &req)
{
    std::string body_data(req.body);

    // Giải mã dữ liệu nhận được bằng thuật toán RC4
    Http::RC4("VDTSecretKey", body_data);
    std::cout << "[Secure API] Decrypted body: " << body_data << "\n";

    // Chuẩn bị phản hồi JSON
    std::string resp = "{\"status\":\"ok\",\"secret_received\":\"" + body_data + "\"}";

    // Mã hóa phản hồi trước khi gửi về client
    Http::RC4("VDTSecretKey", resp);

    // Gửi phản hồi với header X-Encrypt báo hiệu cho client biết dữ liệu đã được mã hóa
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: application/octet-stream\r\n"
        << "Content-Length: " << resp.size() << "\r\n"
        << "X-Encrypt: true\r\n"
        << "Connection: keep-alive\r\n\r\n"
        << resp;
    std::string response = oss.str();
    send(fd, response.data(), response.size(), MSG_NOSIGNAL);
}

int main()
{
    ServerConfig cfg;
    cfg.port = 8081;
    cfg.num_workers = 2;

    // Đăng ký Route Tải File bằng Wildcard (Khớp mọi file nằm dưới đường dẫn này)
    Http::Router::Register(HttpMethod::GET, "/api/download_image/*", test_download_image);

    // Đăng ký Route POST Upload File lên server
    Http::Router::Register(HttpMethod::POST, "/api/upload", Http::HandleFileUpload);

    // Đăng ký Route POST dữ liệu bảo mật (Mã hóa RC4)
    Http::Router::Register(HttpMethod::POST, "/api/secure_data", secure_data_handler);

    // Phục vụ mọi file tĩnh khác qua Fallback tự động
    Http::Router::RegisterFallback(fallback_test);

    NetworkServer server(cfg);
    global_server = &server;

    // Đăng ký bắt các tín hiệu để tắt server sạch sẽ và nạp lại cấu hình
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGHUP, signal_handler); // Reload config.json không downtime
    std::signal(SIGPIPE, SIG_IGN);

    server.Start();
    return 0;
}
