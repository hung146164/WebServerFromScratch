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
#include <unistd.h>

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
        std::cerr << "[Server] Bind failed!\n";
        return;
    }

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

    std::cout << "[Worker " << worker_id << "] Event Loop started successfully.\n";

    while (true)
    {
        int cnt = epoll_wait(epoll_socket.GetSocketfd(),
                             client_events.data(),
                             config.max_epoll_events,
                             -1);
        if (cnt < 0)
        {
            if (errno == EINTR)
                continue;
            std::cerr << "[Worker " << worker_id << "] epoll_wait error\n";
            break;
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
}

void Worker::CloseConnection(int fd)
{
    epoll_ctl(epoll_socket.GetSocketfd(), EPOLL_CTL_DEL, fd, nullptr);
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
                // dữ liệu chưa hoàn chỉnh
                break;
        }

        if (connection_closed)
            return;
    }
}
