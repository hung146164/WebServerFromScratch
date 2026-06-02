#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <random>

using namespace std;
using namespace std::chrono;

// --- Cấu hình ---
const char *SERVER_IP = "104.248.150.184";
const int SERVER_PORT = 8080;
const int TOTAL_CONNECTIONS = 500; // Giảm số lượng xuống một chút để tập trung vào test Logic Parser

// Kho "vũ khí" Chaos (Các loại payload dị biệt)
const vector<string> CHAOS_PAYLOADS = {
    // 1. Gói chuẩn chỉ
    "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n",

    // 2. Dính lẹo (Pipelining): 3 Request dính liền nhau trong 1 lần gửi.
    // Yêu cầu Parser phải cắt đủ 3 lần trong 1 vòng while(true).
    "GET /1 HTTP/1.1\r\n\r\nGET /2 HTTP/1.1\r\n\r\nPOST /3 HTTP/1.1\r\nContent-Length: 0\r\n\r\n",

    // 3. Đột biến Header: Viết hoa thường lộn xộn, thừa dấu cách.
    "pOsT /api HtTp/1.1\r\nhOsT:   localhost  \r\nCoNtEnT-LeNgTh: 0\r\n\r\n",

    // 4. Gói tin độc hại: Báo Content-Length cực lớn để lừa server xin RAM (OOM)
    "PUT /hack HTTP/1.1\r\nContent-Length: 9999999999\r\n\r\n"};

struct ConnCtx
{
    int fd;
    steady_clock::time_point start_time;
};

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main()
{
    int epoll_fd = epoll_create1(0);
    vector<ConnCtx *> contexts;
    int success_count = 0;
    int error_count = 0; // Đếm số lần server ngắt kết nối do crash hoặc lỗi

    cout << "🌪️ Khởi động CHAOS BOT. Bắn " << TOTAL_CONNECTIONS << " kết nối tới " << SERVER_IP << "..." << endl;

    for (int i = 0; i < TOTAL_CONNECTIONS; ++i)
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        set_nonblocking(sock);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

        connect(sock, (struct sockaddr *)&addr, sizeof(addr));
        ConnCtx *ctx = new ConnCtx{sock, steady_clock::now()};
        contexts.push_back(ctx);

        struct epoll_event ev;
        ev.events = EPOLLOUT | EPOLLET;
        ev.data.ptr = ctx;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev);
    }

    auto start_test = steady_clock::now();
    struct epoll_event events[TOTAL_CONNECTIONS];
    char recv_buf[8192];

    // Khởi tạo random generator
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dist(0, CHAOS_PAYLOADS.size() - 1);

    while (duration_cast<seconds>(steady_clock::now() - start_test).count() < 10)
    {
        int nfds = epoll_wait(epoll_fd, events, TOTAL_CONNECTIONS, 100);

        for (int n = 0; n < nfds; ++n)
        {
            ConnCtx *ctx = (ConnCtx *)events[n].data.ptr;

            if (events[n].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            {
                error_count++;
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ctx->fd, nullptr);
                close(ctx->fd);
                continue;
            }

            if (events[n].events & EPOLLOUT)
            {
                // Chọn ngẫu nhiên 1 payload để hành hạ Parser
                string payload = CHAOS_PAYLOADS[dist(gen)];
                send(ctx->fd, payload.c_str(), payload.length(), 0);

                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
                ev.data.ptr = ctx;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, ctx->fd, &ev);
            }
            else if (events[n].events & EPOLLIN)
            {
                int len = recv(ctx->fd, recv_buf, sizeof(recv_buf), 0);
                if (len > 0)
                {
                    success_count++;
                    struct epoll_event ev;
                    ev.events = EPOLLOUT | EPOLLET | EPOLLRDHUP;
                    ev.data.ptr = ctx;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, ctx->fd, &ev);
                }
                else if (len == 0)
                {
                    error_count++;
                }
            }
        }
    }

    cout << "\n==============================" << endl;
    cout << "🌪️ KẾT QUẢ CHAOS TEST 🌪️" << endl;
    cout << "==============================" << endl;
    cout << "✅ Requests phản hồi: " << success_count << endl;
    cout << "💀 Kết nối bị ngắt (Lỗi Parser/Crash): " << error_count << endl;

    for (auto ctx : contexts)
    {
        close(ctx->fd);
        delete ctx;
    }
    close(epoll_fd);
    return 0;
}