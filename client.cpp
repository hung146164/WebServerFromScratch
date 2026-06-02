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

using namespace std;
using namespace std::chrono;

// --- Cấu hình ---
const char *SERVER_IP = "YOUR_SERVER_IP"; // Thay bằng IP server Singapore
const int SERVER_PORT = 8080;
const int TOTAL_CONNECTIONS = 1000; // Số lượng bot kết nối song song
const char *PAYLOAD = "GET /test HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n";

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
    if (epoll_fd == -1)
        return 1;

    vector<ConnCtx *> contexts;
    vector<double> latencies;
    int success_count = 0;

    cout << "🚀 Khởi tạo " << TOTAL_CONNECTIONS << " kết nối tới " << SERVER_IP << "..." << endl;

    for (int i = 0; i < TOTAL_CONNECTIONS; ++i)
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
            continue;

        set_nonblocking(sock);

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

        // Kết nối non-blocking
        connect(sock, (struct sockaddr *)&addr, sizeof(addr));

        ConnCtx *ctx = new ConnCtx{sock, steady_clock::now()};
        contexts.push_back(ctx);

        struct epoll_event ev;
        ev.events = EPOLLOUT | EPOLLET; // Chờ sẵn sàng để gửi request
        ev.data.ptr = ctx;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev);
    }

    auto start_test = steady_clock::now();
    struct epoll_event events[TOTAL_CONNECTIONS];
    char recv_buf[4096];

    // Chạy test trong 10 giây
    while (duration_cast<seconds>(steady_clock::now() - start_test).count() < 10)
    {
        int nfds = epoll_wait(epoll_fd, events, TOTAL_CONNECTIONS, 100);

        for (int n = 0; n < nfds; ++n)
        {
            ConnCtx *ctx = (ConnCtx *)events[n].data.ptr;

            if (events[n].events & EPOLLOUT)
            {
                // Gửi request
                send(ctx->fd, PAYLOAD, strlen(PAYLOAD), 0);
                ctx->start_time = steady_clock::now();

                // Chuyển sang chờ nhận phản hồi
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLET;
                ev.data.ptr = ctx;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, ctx->fd, &ev);
            }
            else if (events[n].events & EPOLLIN)
            {
                // Nhận phản hồi
                int len = recv(ctx->fd, recv_buf, sizeof(recv_buf), 0);
                if (len > 0)
                {
                    auto end_time = steady_clock::now();
                    double lat = duration_cast<microseconds>(end_time - ctx->start_time).count() / 1000.0;
                    latencies.push_back(lat);
                    success_count++;

                    // Quay lại gửi tiếp (Keep-alive)
                    struct epoll_event ev;
                    ev.events = EPOLLOUT | EPOLLET;
                    ev.data.ptr = ctx;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, ctx->fd, &ev);
                }
            }
        }
    }

    // --- Báo cáo ---
    double total_time = duration_cast<milliseconds>(steady_clock::now() - start_test).count() / 1000.0;
    cout << "\n"
         << string(30, '=') << "\n📊 KẾT QUẢ TEST\n"
         << string(30, '=') << endl;
    cout << "✅ Requests thành công: " << success_count << endl;
    cout << "🚀 RPS (Avg): " << (success_count / total_time) << endl;

    if (!latencies.empty())
    {
        sort(latencies.begin(), latencies.end());
        cout << "📉 Latency Avg: " << (success_count > 0 ? (total_time * 1000 / success_count) : 0) << " ms" << endl;
        cout << "📈 Latency P95: " << latencies[latencies.size() * 0.95] << " ms" << endl;
    }

    for (auto ctx : contexts)
    {
        close(ctx->fd);
        delete ctx;
    }
    close(epoll_fd);
    return 0;
}