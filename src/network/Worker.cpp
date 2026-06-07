/*!
    \file Worker.cpp
    \brief Worker implementation using epoll event loop
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

static void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        std::cerr << "[Worker] fcntl F_GETFL failed\n";
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        std::cerr << "[Worker] fcntl F_SETFL O_NONBLOCK failed\n";
}

Worker::Worker(int worker_id_, ServerConfig config_)
    : worker_id(worker_id_), config(config_),
      lru(config_.client_per_worker)
{
    SetupWorker();
}

void Worker::SetupWorker()
{
    epollSocket.SetSocketfd(epoll_create1(0));
    if (epollSocket.GetSocketfd() == -1)
    {
        std::cerr << "[Worker " << worker_id << "] epoll_create1 failed\n";
        return;
    }
    client_events.resize((int)config.max_epoll_events);
}

void Worker::StartWorker()
{
    if (epollSocket.GetSocketfd() == -1)
        return;

    while (true)
    {
        int cnt = epoll_wait(epollSocket.GetSocketfd(),
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

            if (client_events[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP))
            {
                CloseConnection(fd);
            }
            else if (client_events[i].events & EPOLLIN)
            {
                HandleRequest(fd);
            }
        }
    }
}

void Worker::CloseConnection(int fd)
{
    epoll_ctl(epollSocket.GetSocketfd(), EPOLL_CTL_DEL, fd, nullptr);
    close(fd);

    if (lru.remove(fd))
    {
        current_conn--;
    }
}

void Worker::AddClient(int client_fd)
{
    if (lru.full())
    {
        int old_fd = lru.oldestKey();
        if (old_fd != -1)
        {
            CloseConnection(old_fd);
        }
    }

    set_nonblocking(client_fd);

    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.data.fd = client_fd;

    if (epoll_ctl(epollSocket.GetSocketfd(), EPOLL_CTL_ADD, client_fd, &ev) < 0)
    {
        std::cerr << "[Worker " << worker_id << "] epoll_ctl ADD failed\n";
        close(client_fd);
        return;
    }

    lru.put(client_fd);

    current_conn++;
}

void Worker::ProcessHttpRequest(int fd, HttpRequest *request)
{
    Http::Router::Dispatch(fd, *request);
}

void Worker::SendStatusResponse(int fd, int status_code, std::string_view msg)
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << msg << "\r\n"
        << "Content-Type: text/plain\r\n"
        << "Content-Length: " << msg.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << msg;
    std::string res = oss.str();
    send(fd, res.data(), res.size(), MSG_NOSIGNAL);
}

void Worker::HandleRequest(int fd)
{
    HttpRequest *request = lru.get(fd);
    if (!request)
        return;

    int capacity = (int)request->cache.size(); // buffer 64KB, int e du

    while (true)
    {
        int space = capacity - request->tail_idx;
        if (space <= 0)
        {
            SendStatusResponse(fd, 413, "Payload Too Large");
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
                continue;
            }
            else if (state == ErrorState::Instance())
            {
                Http::Error(fd, 400, "Bad Request");
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

int Worker::getConnCount()
{
    return current_conn.load();
}
