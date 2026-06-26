#include "network/NetworkServer.h"
#include "common/Json/JsonParser.h"
#include "common/Json/JsonDocument.h"
#include "common/Json/JsonNode.h"
#include <chrono>
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

// Handler minh họa phân tích cú pháp và xử lý JSON (Thread-Local Arena Parser Demo)
void parse_students_handler(int fd, const HttpRequest &req)
{
    // Cấp phát trước 1,000 nodes trên mỗi luồng (Size Guard chống DoS)
    thread_local Json::JsonParser parser(1000);

    try
    {
        auto start = std::chrono::high_resolution_clock::now();

        Json::JsonDocument doc = parser.Parse(req.body);

        Json::JsonNode *root = doc.GetNode();
        if (!root || root->type != Json::JsonType::ARRAY)
        {
            Http::JSON(fd, 400, "{\"error\":\"Invalid JSON. Expected an array of student objects.\"}");
            return;
        }

        int total_students = 0;
        double sum_gpa = 0.0;
        double max_gpa = -1.0;
        std::string top_student = "";

        Json::JsonNode *curr = root->child;
        while (curr != nullptr)
        {
            if (curr->type == Json::JsonType::OBJECT)
            {
                std::string name = "";
                int age = 0;
                double gpa = 0.0;

                Json::JsonNode *prop = curr->child;
                while (prop != nullptr)
                {
                    if (prop->key == "name" && prop->type == Json::JsonType::STRING)
                    {
                        name = std::string(prop->value);
                    }
                    else if (prop->key == "age" && prop->type == Json::JsonType::NUMBER)
                    {
                        age = std::stoi(std::string(prop->value));
                    }
                    else if (prop->key == "gpa" && prop->type == Json::JsonType::NUMBER)
                    {
                        gpa = std::stod(std::string(prop->value));
                    }
                    prop = prop->next;
                }

                if (!name.empty() && age > 0)
                {
                    total_students++;
                    sum_gpa += gpa;
                    if (gpa > max_gpa)
                    {
                        max_gpa = gpa;
                        top_student = name;
                    }
                }
            }
            curr = curr->next;
        }

        auto end = std::chrono::high_resolution_clock::now();
        double elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        if (total_students == 0)
        {
            Http::JSON(fd, 400, "{\"error\":\"No valid student records found.\"}");
            return;
        }

        double avg_gpa = sum_gpa / total_students;

        std::string resp = "{"
                           "\"status\":\"success\","
                           "\"total_students\":" +
                           std::to_string(total_students) + ","
                                                            "\"average_gpa\":" +
                           std::to_string(avg_gpa) + ","
                                                     "\"top_student\":\"" +
                           top_student + "\","
                                         "\"max_gpa\":" +
                           std::to_string(max_gpa) + ","
                                                     "\"parsing_time_us\":" +
                           std::to_string(elapsed_us) + ","
                                                        "\"node_limit\":10000"
                                                        "}";
        Http::JSON(fd, 200, resp);
    }
    catch (const std::exception &e)
    {
        std::string err_msg = e.what();
        if (err_msg.find("Memory pool exhausted") != std::string::npos)
        {
            Http::JSON(fd, 413, "{\"error\":\"Payload Too Large: JSON structure exceeds the 10000 node pool security limit!\"}");
        }
        else
        {
            Http::JSON(fd, 400, "{\"error\":\"Bad Request: JSON parsing error or malformed structure.\"}");
        }
    }
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

    // Đăng ký Route POST xử lý danh sách học sinh (JSON Parser demo)
    Http::Router::Register(HttpMethod::POST, "/api/parse_students", parse_students_handler);

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
