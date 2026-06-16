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
    server_socket.SetSocketfd(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));

    if (server_socket.GetSocketfd() < 0)
    {
        std::cerr << "[Server] Socket not initialized!\n";
        return;
    }

    int opt = 1;
    if (setsockopt(server_socket.GetSocketfd(), SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "[Server] Set socket SO_REUSEPORT failed!\n";
        return;
    }

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

    client_events.resize((int)config.max_epoll_events);
}
void Worker::StartWorker()
{
    if (epoll_socket.GetSocketfd() == -1)
        return;

    current_worker_id = worker_id;

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
