/*!
    \file main.cpp
    \brief Demo - English Center Management with JSON file persistence
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/

#include "network/NetworkServer.h"
#include "network/ServerConfig.h"
#include "server/http/Router.h"
#include "server/http/HttpResponse.h"
#include "server/http/HttpStatic.h"
#include "server/http/HttpUtils.h"
#include "server/http/JsonParser.h"
#include "server/http/HttpMethod.h"

#include <fstream>
#include <sstream>
#include <mutex>
#include <filesystem>
#include <cstdio>

// ─── Config ──────────────────────────────────────────────────────────────────
static const std::string DATA_DIR = "/home/hung146164/WebServerVdt/data";
static const std::string DATA_FILE = DATA_DIR + "/students.json";
static std::mutex g_mutex;

// ─── File helpers ─────────────────────────────────────────────────────────────

static std::string ReadFile()
{
    std::ifstream f(DATA_FILE);
    if (!f.is_open())
        return "[]";
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    return s.empty() ? "[]" : s;
}

static void WriteFile(const std::string &content)
{
    std::filesystem::create_directories(DATA_DIR);
    std::ofstream f(DATA_FILE, std::ios::trunc);
    f << content;
}

// ─── Build JSON string thủ công (an toàn, không cần JsonBuilder) ─────────────

// Escape string cho JSON
static std::string JsonStr(const std::string &s)
{
    std::string out = "\"";
    for (char c : s)
    {
        if (c == '"')
            out += "\\\"";
        else if (c == '\\')
            out += "\\\\";
        else if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            out += "\\r";
        else if (c == '\t')
            out += "\\t";
        else
            out += c;
    }
    out += "\"";
    return out;
}

// Tạo 1 student JSON object
static std::string MakeStudent(int id, const std::string &name,
                               const std::string &cls, double score)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f", score);
    return "{\"id\":" + std::to_string(id) +
           ",\"name\":" + JsonStr(name) +
           ",\"class\":" + JsonStr(cls) +
           ",\"score\":" + std::string(buf) + "}";
}

// ─── Handlers ────────────────────────────────────────────────────────────────

void HandleGetStudents(int fd, const HttpRequest &req)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    Http::JSON(fd, 200, ReadFile());
}

void HandleAddStudent(int fd, const HttpRequest &req)
{
    if (!Http::IsJson(req))
    {
        Http::Error(fd, 415, "Only application/json accepted");
        return;
    }
    if (req.body.empty())
    {
        Http::Error(fd, 400, "Empty body");
        return;
    }

    // Parse body
    Http::JsonNode *body = Http::JsonParser::Parse(req.body);
    if (!body)
    {
        Http::Error(fd, 400, "Invalid JSON");
        return;
    }
    if (!Http::JsonParser::Has(body, "name"))
    {
        Http::JsonParser::Free(body);
        Http::Error(fd, 400, "Missing field: name");
        return;
    }

    // Copy dữ liệu vào std::string ngay, tránh string_view dangling
    std::string name = std::string(Http::JsonParser::GetString(body, "name"));
    std::string cls = std::string(Http::JsonParser::GetString(body, "class"));
    double score = Http::JsonParser::GetDouble(body, "score");
    Http::JsonParser::Free(body); // Giải phóng cây parse

    std::lock_guard<std::mutex> lock(g_mutex);

    // Đọc file, tìm max ID
    std::string existing = ReadFile();
    Http::JsonNode *arr = Http::JsonParser::Parse(existing);
    int max_id = 0;
    if (arr && arr->type == Http::JsonType::ARRAY)
    {
        Http::JsonNode *curr = arr->child;
        while (curr)
        {
            int id = Http::JsonParser::GetInt(curr, "id");
            if (id > max_id)
                max_id = id;
            curr = curr->next;
        }
    }
    Http::JsonParser::Free(arr); // Giải phóng sau khi đã dùng xong

    int new_id = max_id + 1;

    // Ghép array mới = existing (bỏ ']' cuối) + student mới + ']'
    std::string student_json = MakeStudent(new_id, name, cls, score);
    std::string new_array;

    // Trim whitespace cuối existing
    int end = (int)existing.find_last_not_of(" \t\n\r");
    if (end != (int)std::string::npos)
        existing = existing.substr(0, (size_t)(end + 1));

    if (existing == "[]")
    {
        // Array rỗng → thêm phần tử đầu tiên
        new_array = "[" + student_json + "]";
    }
    else
    {
        // existing = "[...items...]" → bỏ ']' cuối và thêm vào
        new_array = existing.substr(0, existing.size() - 1) + "," + student_json + "]";
    }

    WriteFile(new_array);

    // Response JSON
    std::string response = "{\"status\":\"created\","
                           "\"id\":" +
                           std::to_string(new_id) +
                           ",\"name\":" + JsonStr(name) +
                           ",\"class\":" + JsonStr(cls) +
                           ",\"score\":" + [&]()
    {
        char b[32];
        snprintf(b, 32, "%.1f", score);
        return std::string(b);
    }() + "}";

    Http::JSON(fd, 201, response);
}

void HandleUpdateStudent(int fd, const HttpRequest &req)
{
    if (!Http::IsJson(req))
    {
        Http::Error(fd, 415, "Only application/json accepted");
        return;
    }
    if (req.body.empty())
    {
        Http::Error(fd, 400, "Empty body");
        return;
    }

    // Parse body
    Http::JsonNode *body = Http::JsonParser::Parse(req.body);
    if (!body)
    {
        Http::Error(fd, 400, "Invalid JSON");
        return;
    }
    if (!Http::JsonParser::Has(body, "id"))
    {
        Http::JsonParser::Free(body);
        Http::Error(fd, 400, "Missing field: id");
        return;
    }

    int target_id = Http::JsonParser::GetInt(body, "id");
    std::string name = std::string(Http::JsonParser::GetString(body, "name"));
    std::string cls = std::string(Http::JsonParser::GetString(body, "class"));
    double score = Http::JsonParser::GetDouble(body, "score");
    Http::JsonParser::Free(body); // Giải phóng body

    std::lock_guard<std::mutex> lock(g_mutex);

    std::string existing = ReadFile();
    Http::JsonNode *arr = Http::JsonParser::Parse(existing);
    if (!arr || arr->type != Http::JsonType::ARRAY)
    {
        Http::JsonParser::Free(arr);
        Http::Error(fd, 404, "No data");
        return;
    }

    // Build new array cập nhật student có id = target_id
    std::string new_array = "[";
    bool found = false;
    bool first = true;
    Http::JsonNode *curr = arr->child;
    while (curr)
    {
        int id = Http::JsonParser::GetInt(curr, "id");
        if (!first)
            new_array += ",";
        first = false;

        if (id == target_id)
        {
            found = true;
            new_array += MakeStudent(id, name, cls, score);
        }
        else
        {
            std::string n = std::string(Http::JsonParser::GetString(curr, "name"));
            std::string c = std::string(Http::JsonParser::GetString(curr, "class"));
            double s = Http::JsonParser::GetDouble(curr, "score");
            new_array += MakeStudent(id, n, c, s);
        }
        curr = curr->next;
    }
    new_array += "]";
    Http::JsonParser::Free(arr);

    if (!found)
    {
        Http::Error(fd, 404, "Student not found");
        return;
    }

    WriteFile(new_array);

    // Response JSON
    std::string response = "{\"status\":\"updated\","
                           "\"id\":" +
                           std::to_string(target_id) +
                           ",\"name\":" + JsonStr(name) +
                           ",\"class\":" + JsonStr(cls) +
                           ",\"score\":" + [&]()
    {
        char b[32];
        snprintf(b, 32, "%.1f", score);
        return std::string(b);
    }() + "}";

    Http::JSON(fd, 200, response);
}

void HandleDeleteStudent(int fd, const HttpRequest &req)
{
    auto id_sv = Http::QueryParam(req, "id");
    if (id_sv.empty())
    {
        Http::Error(fd, 400, "Missing query param: id");
        return;
    }
    int target_id = std::atoi(std::string(id_sv).c_str());

    std::lock_guard<std::mutex> lock(g_mutex);

    std::string existing = ReadFile();
    Http::JsonNode *arr = Http::JsonParser::Parse(existing);
    if (!arr || arr->type != Http::JsonType::ARRAY)
    {
        Http::JsonParser::Free(arr);
        Http::Error(fd, 404, "No data");
        return;
    }

    // Build new array bỏ qua student có id = target_id
    std::string new_array = "[";
    bool found = false;
    bool first = true;
    Http::JsonNode *curr = arr->child;
    while (curr)
    {
        int id = Http::JsonParser::GetInt(curr, "id");
        if (id != target_id)
        {
            std::string n = std::string(Http::JsonParser::GetString(curr, "name"));
            std::string c = std::string(Http::JsonParser::GetString(curr, "class"));
            double s = Http::JsonParser::GetDouble(curr, "score");
            if (!first)
                new_array += ",";
            new_array += MakeStudent(id, n, c, s);
            first = false;
        }
        else
            found = true;
        curr = curr->next;
    }
    new_array += "]";
    Http::JsonParser::Free(arr);

    if (!found)
    {
        Http::Error(fd, 404, "Student not found");
        return;
    }

    WriteFile(new_array);
    Http::NoContent(fd);
}

void HandleHealthCheck(int fd, const HttpRequest &req)
{
    (void)req;
    Http::Text(fd, 200, "pong");
}

void HandleStatic(int fd, const HttpRequest &req)
{
    Http::ServeFile(fd, req.http_url, "/home/hung146164/WebServerVdt/www");
}

// ─── main ────────────────────────────────────────────────────────────────────
signed main()
{
    std::filesystem::create_directories(DATA_DIR);

    ServerConfig cfg;
    cfg.port = 8081;
    cfg.num_workers = 2;
    cfg.client_per_worker = 200;
    cfg.max_epoll_events = 512;

    Http::Router::Register(HttpMethod::GET, "/api/students", HandleGetStudents);
    Http::Router::Register(HttpMethod::POST, "/api/students", HandleAddStudent);
    Http::Router::Register(HttpMethod::PUT, "/api/students", HandleUpdateStudent);
    Http::Router::Register(HttpMethod::DELETE, "/api/students", HandleDeleteStudent);
    Http::Router::Register(HttpMethod::GET, "/health", HandleHealthCheck);
    Http::Router::RegisterFallback(HandleStatic);

    NetworkServer server(cfg);
    server.Start();
    return 0;
}
