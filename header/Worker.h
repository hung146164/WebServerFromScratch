#include "Network_Common.h"
#include "SocketGuard.h"
#include "ParseHttp.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

class Worker
{
private:
    // Network
    SocketGuard serverSocket;
    SocketGuard epollSocket;
    epoll_event configEvent;
    std::vector<epoll_event> client_events;

    std::unordered_map<uint32_t, char> IP_MAP;

    int port = -1;
    int max_listen_event = -1;
    int max_epoll_event = -1;
    int max_client_per_ip = 0;
    int max_client = 0;

    // Thread
    int worker_id = -1;

    void SetupSocket()
    {
        serverSocket.SetSocketfd(socket(AF_INET, SOCK_STREAM, 0));
        if (serverSocket.GetSocketfd() < 0)
        {
            std::cerr << "Create socket failed: " << worker_id << '\n';
            return;
        }
        // Struct chứa thông tin địa chỉ mạng
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        /* setsocketopt(
        socket muốn cấu hình,
        option này thuộc tầng nào, có SOL_SOCKET: option chung, tầng ip, tầng tcp
        optname: SO_REUSEADDR: bind lại port nhanh, SO_REUSEPORT: nhiều socket chung port
        opt,
        size opt.
        )

        */
        // hiện đang cấu hình cho phép nhiều thread cùng bind vào một port
        int opt = 1;
        if (setsockopt(serverSocket.GetSocketfd(), SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
        {
            std::cerr << "Set socket REUSEPORT failed: " << worker_id << '\n';
            return;
        }

        if (bind(serverSocket.GetSocketfd(), (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            std::cerr << "Bind failed: " << worker_id << '\n';
            return;
        }

        if (listen(serverSocket.GetSocketfd(), max_listen_event) < 0)
        {
            std::cerr << "Listen failed: " << worker_id << '\n';
            return;
        }

        set_nonblocking(serverSocket.GetSocketfd());

        epollSocket.SetSocketfd(epoll_create1(0));

        configEvent.events = EPOLLIN | EPOLLET;
        configEvent.data.fd = serverSocket.GetSocketfd();

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
        if (epoll_ctl(epollSocket.GetSocketfd(), EPOLL_CTL_ADD, serverSocket.GetSocketfd(), &configEvent))
        {
            std::cerr << "Epoll ctl failed: " << worker_id << '\n';
            return;
        }

        client_events.resize(max_epoll_event);

        std::cout << worker_id << " Set up success!" << '\n';
    }
    void AcceptNewClient()
    {
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        while (true)
        {
            /*
                Khi ông nhận accept hệ điều hành sẽ tạo fd cho kết nối đó
            */
            int client_fd = accept(serverSocket.GetSocketfd(), (sockaddr *)&clientAddr, &clientAddrLen);

            if (client_fd < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                if (errno == EMFILE || errno == ENFILE)
                    std::cerr << "FATAL: fd limit reached\n";
                else
                    std::cerr << "Bug in accept client << " << errno << '\n';

                break;
            }
            if (IP_MAP[clientAddr.sin_addr.s_addr] >= max_client_per_ip)
            {
                close(client_fd);
                continue;
            }
            set_nonblocking(client_fd);
            struct epoll_event client_ev;
            client_ev.events = EPOLLIN | EPOLLET;
            client_ev.data.fd = client_fd;

            if (epoll_ctl(epollSocket.GetSocketfd(), EPOLL_CTL_ADD, client_fd, &client_ev) < 0)
            {
                std::cerr << "Epoll ctl add client failed: " << worker_id << " errno=" << errno << '\n';
                close(client_fd);
                continue;
            }
        }
    }

    std::string BuildHttpResponse(const HttpRequest &request)
    {
    }

    void HandleRequest(int fd)
    {
    }

public:
    Worker(int workerid, int port, int max_listen_event, int max_epoll_event, int max_client_per_ip, int max_client)
    {
        this->worker_id = workerid;
        this->port = port;
        this->max_listen_event = max_listen_event;
        this->max_epoll_event = max_epoll_event;
        this->max_client_per_ip = max_client_per_ip;
        this->max_client = max_client;
        SetupSocket();
    }
    void StartWorker()
    {
        if (port == -1 || max_listen_event == -1 || max_epoll_event == -1)
        {
            return;
        }
        if (worker_id == -1)
        {
            return;
        }
        std::cout << "Worker: " << worker_id << " bắt đầu lắng nghe port " << port << '\n';
        while (true)
        {
            /*
                đoạn này có nghĩa là hệ điều hành sẽ xem có bao nhiêu ông có biến rồi đưa các
                ông đó vào danh sách này, bắt đầu từ 0 -> cnt-1. vậy nên mỗi lần epoll_wait
                trả về thì cái events buffer đã bị ghi đè.

                timeout: -1: cứ ngủ lúc nào có thì báo, x: chờ tối đa x milisecond rồi kiểm tra.
            */
            int cnt = epoll_wait(epollSocket.GetSocketfd(), client_events.data(), max_epoll_event, -1);

            for (int i = 0; i < cnt; i++)
            {
                if (client_events[i].data.fd == serverSocket.GetSocketfd())
                {
                    // Nhận connect mới
                    /*
                        HDH: tôi vừa bắt tay được vài thằng client.
                    */
                    AcceptNewClient();
                }
                else
                {
                    // Xử lý client cũ
                    /*
                        HDH: có vài đứa mới gửi request
                    */
                    HandleRequest(client_events[i].data.fd);
                }
            }
        }
    }
};