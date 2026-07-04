#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <filesystem>
#include <cstdio>
#include <csignal>
#include <chrono>
#include <unistd.h>

#include "network/NetworkServer.h"
#include "common/Json/JsonParser.h"
#include "common/Json/JsonDocument.h"
#include "common/Json/JsonNode.h"
#include "network/ServerConfig.h"
#include "server/http/Router.h"
#include "server/http/HttpResponse.h"
#include "server/http/HttpStatic.h"
#include "server/http/HttpUtils.h"
#include "server/http/HttpMethod.h"

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

void view_handler(int fd, const HttpRequest &request)
{
    std::string_view url_to_serve = request.http_url;
    if (url_to_serve.rfind("/api/view", 0) == 0)
    {
        url_to_serve.remove_prefix(9);
    }
    if (url_to_serve.empty())
    {
        url_to_serve = "/";
    }
    Http::ServeFile(fd, request, url_to_serve, "www/view", true);
}

void download_handler(int fd, const HttpRequest &request)
{
    std::string_view url_to_serve = request.http_url;
    if (url_to_serve.rfind("/api/download", 0) == 0)
    {
        url_to_serve.remove_prefix(13);
    }
    if (url_to_serve.empty())
    {
        url_to_serve = "/";
    }
    Http::ServeFile(fd, request, url_to_serve, "www/download", false);
}

void fallback_test(int fd, const HttpRequest &request)
{
    Http::ServeFile(fd, request, request.http_url, "www/view", true);
}

void parse_students_handler(int fd, const HttpRequest &req)
{
    thread_local Json::JsonParser parser(5000);

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

    // API hiển thị/xem file trực tiếp
    Http::Router::Register(HttpMethod::GET, "/api/view/*", view_handler);

    // API tải file về máy (attachment)
    Http::Router::Register(HttpMethod::GET, "/api/download/*", download_handler);

    // API upload
    Http::Router::Register(HttpMethod::POST, "/api/upload", Http::HandleFileUpload);

    // API parse json
    Http::Router::Register(HttpMethod::POST, "/api/parse_students", parse_students_handler);

    // API Fallback
    Http::Router::RegisterFallback(fallback_test);

    NetworkServer server(cfg);
    global_server = &server;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGHUP, signal_handler);
    std::signal(SIGPIPE, SIG_IGN);

    server.Start();
    return 0;
}