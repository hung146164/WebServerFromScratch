#include "server/http/Router.h"
#include "server/http/HttpUtils.h"
#include <sys/socket.h>
#include <iostream>
#include <sstream>
#include <ctime>
#include <unordered_map>

std::unordered_map<std::string, HandlerFunc> Http::Router::routes;
HandlerFunc Http::Router::fallback_handler = nullptr;
int Http::Router::rate_limit_per_sec = 20;

// Khai báo biến thread-local của worker ID (được định nghĩa trong Worker.cpp)
extern thread_local int current_worker_id;

// Cấu trúc phục vụ cho bộ giới hạn tần suất yêu cầu (Rate Limiting)
struct RateLimitInfo
{
    int count = 0;
    time_t window_start = 0;
};

/*
    mỗi luồng sẽ có một cái để phục vụ, ko global tránh mutex
*/
thread_local std::unordered_map<std::string, RateLimitInfo> ip_rate_limits;

std::string Http::Router::MethodToString(HttpMethod method)
{
    switch (method)
    {
    case HttpMethod::GET:
        return "GET";
    case HttpMethod::POST:
        return "POST";
    case HttpMethod::PUT:
        return "PUT";
    case HttpMethod::DELETE:
        return "DELETE";
    case HttpMethod::HEAD:
        return "HEAD";
    case HttpMethod::OPTIONS:
        return "OPTIONS";
    default:
        return "UNKNOWN";
    }
}

void Http::Router::Register(HttpMethod method, const std::string &path, HandlerFunc handler)
{
    std::string key = MethodToString(method) + ":" + path;
    routes[key] = handler;
    std::cout << "[Router] Registered: " << key << "\n";
}

void Http::Router::RegisterFallback(HandlerFunc handler)
{
    fallback_handler = handler;
    std::cout << "[Router] Registered fallback handler\n";
}

void Http::Router::Dispatch(int fd, const HttpRequest &req)
{
    /*
        Kiểm tra tần xuất yêu cầu
        // Giới hạn mỗi giây 1 socket chỉ có thể gửi
        tối đa rate_limit_per_sec request
    */
    time_t now = time(nullptr);
    auto &limit_info = ip_rate_limits[std::string(req.client_ip)];

    /*
        nếu thời gian >=1s thì reset lại biến đếm
    */
    if (now - limit_info.window_start >= 1)
    {
        limit_info.count = 1;
        limit_info.window_start = now;
    }
    else
    {
        // Kiểm tra nếu request vượt quá mức cho phép sẽ fail khi gửi tránh DOSS
        limit_info.count++;
        if (limit_info.count > rate_limit_per_sec)
        {
            SendErrorResponse(fd, 429, "Too Many Requests", "{\"error\":\"Rate limit exceeded. Max " + std::to_string(rate_limit_per_sec) + " req/sec.\"}");
            Http::LogRequest(current_worker_id, req.client_ip, req, 429);
            return;
        }
    }

    // Xu ly OPTIONS preflight CORS

    /*
        Tại sao cần cái này?
        Vì trình duyệt chrome hay edge hay bất cứ trình duyệt nào đều có
        cơ chế bảo mật gọi là Same-Origin Policy,
        nghĩa là mặc định chặn JavaScript từ domain A gọi API đến domain B.

        cho nên: Trước khi gửi request thật (GET/POST),
        trình duyệt tự động gửi 1 request thăm dò trước bằng method OPTIONS

        ví dụ :
        OPTIONS /api/upload HTTP/1.1
        Origin: http://forredev.me
        Access-Control-Request-Method: POST
        Access-Control-Request-Headers: Content-Type

        Ý nghĩa: "Này server, tao sắp gửi POST với Content-Type, mày có cho phép không?"
        Nếu server không trả lời hoặc trả lời sai,
        trình duyệt hủy request thật luôn, không gửi nữa
    */
    if (req.method == HttpMethod::OPTIONS)
    {
        std::string res =
            "HTTP/1.1 200 OK\r\n"
            "Access-Control-Allow-Origin: *\r\n"                                // Cho phép mọi origin
            "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n" // Các method được phép
            "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"     // Các header được phép
            "Content-Length: 0\r\n\r\n";
        send(fd, res.data(), res.size(), MSG_NOSIGNAL);
        Http::LogRequest(current_worker_id, req.client_ip, req, 200);
        return;
    }

    /*
        Giải mã Url trình duyệt/client tự động encode URL khi có ký tự đặc biệt
        (dấu cách, tiếng Việt, /...). Nếu không decode thì
        my%20file.pdf sẽ không khớp với file thật tên my file.pdf.
    */

    std::string path = Http::UrlDecode(req.http_url);
    std::string key = MethodToString(req.method) + ":" + path;

    /*
        Khi đã lấy được phương thức và URL sẽ check xem là đường dẫn
        tuyết đối hay tương đối
    */
    auto it = routes.find(key);
    if (it != routes.end())
    {
        it->second(fd, req);
        Http::LogRequest(current_worker_id, req.client_ip, req, 200);
        return;
    }

    /*
        Khớp tiền tố  Wildcard / Prefix Match, thì sẽ duyệt các
        route đuôi có dấu * ví du: GET:/api/download/*
    */
    for (const auto &pair : routes)
    {
        const std::string &route_key = pair.first;
        if (!route_key.empty() && route_key.back() == '*')
        {
            std::string prefix = route_key.substr(0, route_key.size() - 1);
            if (key.size() >= prefix.size() && key.compare(0, prefix.size(), prefix) == 0)
            {
                pair.second(fd, req);
                Http::LogRequest(current_worker_id, req.client_ip, req, 200);
                return;
            }
        }
    }

    // không khớp gì cả thì trả về trang chủ :3
    if (fallback_handler)
    {
        fallback_handler(fd, req);
        Http::LogRequest(current_worker_id, req.client_ip, req, 200);
        return;
    }

    // 404
    SendErrorResponse(fd, 404, "Not Found", "{\"error\":\"Route Not Found\"}");
    Http::LogRequest(current_worker_id, req.client_ip, req, 404);
}

void Http::Router::SendErrorResponse(int fd, int status_code,
                                     const std::string &status_msg,
                                     const std::string &body)
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_msg << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Connection: keep-alive\r\n\r\n"
        << body;
    std::string response = oss.str();
    send(fd, response.data(), response.size(), MSG_NOSIGNAL);
}
