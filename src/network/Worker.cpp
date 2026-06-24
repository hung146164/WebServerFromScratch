/*!
    \file Worker.cpp
    \brief Worker implementation
    \author HungForre
    \date 6/6/2026
    \copyright VDT
*/
#include "network/Worker.h"
#include "server/http/HttpParserState.h"
#include "server/http/HttpResponse.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>

thread_local int current_worker_id = -1;

Worker::Worker(int worker_id_, ServerConfig config_)
    : worker_id(worker_id_), config(config_),
      lru(config_.client_per_worker)
{
    SetupNetwork();
}
void Worker::SetupNetwork()
{
    /*
        -Tạo socket ipv4 và sock_stream(tcp), đặt socket ở chế độ ko chặn (nghĩa là nếu
        chưa có gì thì hệ điều hành sẽ trả về mã EAGAIN hoặc EWOULDBLOCK bảo là chưa có gì mới đâu),
        -cờ SOCK_CLOEXEC:
        vấn đề: khi tiến trình con tạo, nó sẽ sao chép nguyên cái des từ cha sang con lúc này
        con cũng trỏ vào socket, giả sử lúc này cha bị kill, Reference count giảm từ 2 xuống 1, vì RC =1
        nên hdh sẽ không thu hỏi port này -> port sẽ bị chiếm dụng.
        giải pháp: khi gắn cờ này nó sẽ tự động xóa ô socket trong FD của con đi.
    */
    server_socket.SetSocketfd(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));

    if (server_socket.GetSocketfd() < 0)
    {
        std::cerr << "[Server] Socket not initialized!\n";
        return;
    }

    /*
        -Cờ SO_REUSEPORT cho phép nhiều tiến trình bind vào một port, dùng hash để phân phối,
        chỗ này mình sẽ tối ưu phân phối sau bằng ./p2c_balancer.c
        -opt=1 là bật , 0 là tắt. (chả hiểu sao c++ yêu cầu truyền hẳn 2 tham số vào làm gì?:D )

    */
    int opt = 1;
    if (setsockopt(server_socket.GetSocketfd(), SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "[Server] Set socket SO_REUSEPORT failed!\n";
        return;
    }

    /*
        chỗ này chỉ thiết lập cấu hình thôi
    */
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)config.port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket.GetSocketfd(), (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "[Server] Bind failed on port " << config.port << "!\n";
        return;
    }
    std::cout << "[Worker " << worker_id << "] Bound and listening on port " << config.port << "\n";

    if (listen(server_socket.GetSocketfd(), config.max_listen_queue) < 0)
    {
        std::cerr << "[Server] Listen failed!\n";
        return;
    }

    /*
        Tạo epoll để quản lý sk mạng, cái này kiểu hdh sẽ gom lại những thẳng muốn phát biểu
        rồi đưa tôi. ở đây có 2 chế độ là
        Level-trigger(hệ điều hành sẽ gọi liên tục,cứ lúc
        nào có biến),
        -Edge-Triggered (lúc nào có biến tạo gọi m đúng 1 lần, do đó khi lấy dữ liệu
        thì phải dùng while để đọc hết, và hàm read là cái hàm sẽ block bạn, cho nên bạn phải set
        cho cái socket là Non block thì mới dùng đc)
        -C++ dùng cái epoll_event, hay sockaddr để vừa gửi cũng như vừa đọc rất tiện

    */

    epoll_socket.SetSocketfd(epoll_create1(0));
    if (epoll_socket.GetSocketfd() == -1)
    {
        std::cerr << "[Worker " << worker_id << "] epoll_create1 failed\n";
        return;
    }
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server_socket.GetSocketfd();
    if (epoll_ctl(epoll_socket.GetSocketfd(), EPOLL_CTL_ADD, server_socket.GetSocketfd(), &ev) < 0)
    {
        std::cerr << "[Worker " << worker_id << "] Failed to add server socket to epoll\n";
    }

    /*
        Này Kernel, kiểm tra xem có những Socket nào vừa có dữ liệu mới
        (hoặc có kết nối mới) thì chép tên (File Descriptor) và
        sự kiện của tụi nó vào cái giỏ client_events này hộ tao.
        Tao chuẩn bị sẵn cái giỏ có sức chứa tối đa là config.max_epoll_events chỗ ngồi rồi.

        Cái này tùy cấu hình máy nếu server cpu thấp thì để thấp. thường 1024 cho 1GB ram
    */
    client_events.resize((int)config.max_epoll_events);
}
void Worker::StartWorker()
{
    if (epoll_socket.GetSocketfd() == -1)
        return;

    current_worker_id = worker_id;

    /*
        nếu ko phải hdh mac, vì os rất hay chuyển luồng
        từ nhân này sang nhân khác vì nó tự muốn cân bằng tải, khi chuyển như thế cache L1/L2
        lại phải nạp lại dữ liệu từ ram. cho nên ta khóa nhân đó trên luồng này tối ưu cache luôn.
    */
#ifndef __APPLE__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(worker_id % std::thread::hardware_concurrency(), &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif

    std::cout << "[Worker " << worker_id << "] Event Loop started successfully on CPU Core "
              << (worker_id % std::thread::hardware_concurrency()) << "\n";
    is_running = true;

    while (is_running)
    {
        int cnt = epoll_wait(epoll_socket.GetSocketfd(),
                             client_events.data(),
                             config.max_epoll_events,
                             500); // 500ms timeout
        if (cnt < 0)
        {
            if (errno == EINTR)
                continue;
            std::cerr << "[Worker " << worker_id << "] epoll_wait error\n";
            break;
        }
        if (cnt == 0)
        {

            time_t now = time(nullptr);
            while (true)
            {
                int oldest_fd = lru.OldestKey();
                if (oldest_fd == -1)
                    break;

                time_t last_active = lru.GetLastActiveTime(oldest_fd);
                if (now - last_active > config.read_timeout_sec)
                {
                    std::cout << "[Timeout] Closing idle client fd " << oldest_fd << " on Worker " << worker_id << "\n";
                    CloseConnection(oldest_fd);
                }
                else
                {
                    break;
                }
            }
            continue;
        }

        for (int i = 0; i < cnt; ++i)
        {
            int fd = client_events[i].data.fd;

            if (client_events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                CloseConnection(fd);

                continue;
            }

            if (fd == server_socket.GetSocketfd())
            {
                AcceptClient();
            }
            else
            {
                HandleRequest(fd);
            }
        }
    }
    std::cout << "[Worker " << worker_id << "] Event Loop stopped.\n";
}

void Worker::StopWorker()
{
    is_running = false;
}

void Worker::CloseConnection(int fd)
{
    epoll_ctl(epoll_socket.GetSocketfd(), EPOLL_CTL_DEL, fd, nullptr);

    shutdown(fd, SHUT_WR);
    char buf[128];
    while (recv(fd, buf, sizeof(buf), 0) > 0)
    {
    }

    close(fd);
    lru.Remove(fd);
}

void Worker::AcceptClient()
{
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    while (true)
    {
        int client_fd = accept4(server_socket.GetSocketfd(),
                                (sockaddr *)&client_addr,
                                &client_len,
                                SOCK_NONBLOCK | SOCK_CLOEXEC);

        if (client_fd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            if (errno == EINTR)
                continue;
            if (errno == EMFILE || errno == ENFILE)
                std::cerr << "[Worker " << worker_id << "] Warning: FD limit reached!\n";
            break;
        }

        if (lru.Full())
        {
            int old_fd = lru.OldestKey();
            if (old_fd != -1)
            {
                CloseConnection(old_fd);
            }
        }

        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.fd = client_fd;

        if (epoll_ctl(epoll_socket.GetSocketfd(), EPOLL_CTL_ADD, client_fd, &ev) < 0)
        {
            std::cerr << "[Worker " << worker_id << "] epoll_ctl ADD failed\n";
            close(client_fd);
            continue;
        }

        lru.Put(client_fd);
        HttpRequest *req = lru.GetWithoutMove(client_fd);
        if (req)
        {
            char ip_str[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN))
            {
                req->client_ip = ip_str;
            }
        }
    }
}

void Worker::ProcessHttpRequest(int fd, HttpRequest *request)
{
    Http::Router::Dispatch(fd, *request);
}

void Worker::HandleRequest(int fd)
{
    HttpRequest *request = lru.GetWithoutMove(fd);
    if (!request)
        return;

    int capacity = (int)request->cache.size();

    while (true)
    {
        int space = capacity - request->tail_idx;
        if (space <= 0)
        {
            std::string error_msg = "Payload Too Large";
            Http::Error(fd, 413, error_msg);

            CloseConnection(fd);

            return;
        }

        char *write_ptr = &request->cache[request->tail_idx];
        ssize_t bytes_read = recv(fd, write_ptr, (int)space, 0);

        if (bytes_read < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            if (errno == EINTR)
                continue;
            CloseConnection(fd);
            return;
        }
        else if (bytes_read == 0)
        {
            CloseConnection(fd);
            return;
        }

        request->tail_idx += (int)bytes_read;

        bool connection_closed = false;
        while (true)
        {
            const HttpParserState *state = request->Parse();

            if (state == CompleteState::Instance())
            {
                ProcessHttpRequest(fd, request);

                int remain = request->tail_idx - request->curr_idx;
                if (remain > 0)
                    std::memmove(&request->cache[0],
                                 &request->cache[request->curr_idx],
                                 (int)remain);
                request->NextRequest(remain);
                lru.MakeRecent(fd);
                continue;
            }
            else if (state == ErrorState::Instance())
            {
                std::string error_msg = "Bad Request";
                Http::Error(fd, 400, error_msg);
                CloseConnection(fd);
                connection_closed = true;
                break;
            }
            else

                break;
        }

        if (connection_closed)
            return;
    }
}

void Worker::UpdateConfig(ServerConfig config_)
{
    config.read_timeout_sec = config_.read_timeout_sec;
    config.max_client_per_ip = config_.max_client_per_ip;
}
