
#include "../header/NetworkConfig.h"
#include "../header/NetworkConfig.h"
#include "../header/SocketGuard.h"
#include "../header/Worker.h"
#include <thread>

SocketGuard serverSocket;
SocketGuard epollSocket;
std::vector<Worker> workers;
std::vector<epoll_event> newClients;

std::vector<Worker *> worker_instances;
std::vector<std::thread> threads;
int next_worker = 0;

void AcceptClient()
{
    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    while (true)
    {
        int client_fd = accept(serverSocket.GetSocketfd(), (sockaddr *)&clientAddr, &clientAddrLen);

        if (client_fd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            if (errno == EMFILE || errno == ENFILE)
            {
                std::cerr << "Cảnh báo: Đạt giới hạn file descriptor hệ thống!\n";
            }
            break;
        }

        // --- PHÂN BỔ ROUND-ROBIN ---
        worker_instances[next_worker]->AddClient(client_fd);

        // Chuyển sang worker tiếp theo cho lần sau
        next_worker = (next_worker + 1) % NUMBER_OF_WORKER;
    }
}
void SetupNetwork()
{
    serverSocket.SetSocketfd(socket(AF_INET, SOCK_STREAM, 0));
    if (serverSocket.GetSocketfd() < 0)
    {
        std::cerr << "Create socket failed: " << '\n';
        return;
    }
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    if (setsockopt(serverSocket.GetSocketfd(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "Set socket REUSEPORT failed: " << '\n';
        return;
    }

    if (bind(serverSocket.GetSocketfd(), (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Bind failed: " << '\n';
        return;
    }

    if (listen(serverSocket.GetSocketfd(), MAX_LISTEN) < 0)
    {
        std::cerr << "Listen failed: " << '\n';
        return;
    }
    /* setsocketopt(
       socket muốn cấu hình,
       option này thuộc tầng nào, có SOL_SOCKET: option chung, tầng ip, tầng tcp
       optname: SO_REUSEADDR: bind lại port nhanh, SO_REUSEPORT: nhiều socket chung port
       opt,
       size opt.
       )
    */

    epollSocket.SetSocketfd(epoll_create1(0));
    epoll_event ev;
    ev.data.fd = serverSocket.GetSocketfd();
    ev.events = EPOLLIN | EPOLLET;

    /* int epoll_ctl(
            int epoll_fd,
            int op,
            int fd,
            struct epoll_event *event); // Mảng để hướng event

            dùng để:
            ADD	thêm fd vào epoll
            MOD	sửa event
            DEL	xóa fd
        */
    if (epoll_ctl(epollSocket.GetSocketfd(), EPOLL_CTL_ADD, serverSocket.GetSocketfd(), &ev) < 0)
    {
        std::cerr << "Epoll refactor error!" << '\n';
    }
}

int main()
{
    SetupNetwork();
    newClients.resize(MAX_EVENTS);

    // 1. Khởi tạo 16 Workers
    for (int i = 0; i < NUMBER_OF_WORKER; ++i)
    {
        Worker *w = new Worker(i, MAX_EVENTS, CLIENT_PER_THREAD);
        worker_instances.push_back(w);
    }

    // 2. Chạy luồng cho từng Worker
    for (int i = 0; i < NUMBER_OF_WORKER; ++i)
    {
        threads.emplace_back(&Worker::StartWorker, worker_instances[i]);
    }

    std::cout << "[Server] Đang lắng nghe tại cổng " << PORT << " với " << NUMBER_OF_WORKER << " luồng.\n";

    // 3. Vòng lặp Accept chính
    while (true)
    {
        int cnt = epoll_wait(epollSocket.GetSocketfd(), newClients.data(), MAX_EVENTS, -1);
        for (int i = 0; i < cnt; i++)
        {
            if (newClients[i].data.fd == serverSocket.GetSocketfd())
            {
                AcceptClient();
            }
        }
    }

    return 0;
}