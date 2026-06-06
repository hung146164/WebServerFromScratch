/*!
    \file Worker.cpp
    \brief Worker implementation using Nginx-style event loop
    \author HungForre
    \date 6/6/2026
    \copyright VDT
*/

#include "network/Worker.h"
#include "server/http/HttpParserState.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string>
#include <cstring>

inline void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1)
    {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

Worker::Worker(int worker_id_, int max_epoll_event_, int max_client_)
    : worker_id(worker_id_),
      max_epoll_event(max_epoll_event_),
      max_client(max_client_),
      lru(max_client_)
{
    epollSocket.SetSocketfd(epoll_create1(0));
    client_events.resize(static_cast<size_t>(max_epoll_event_));
}

void Worker::StartWorker()
{
    if (epollSocket.GetSocketfd() == -1)
        return;

    while (true)
    {
        int cnt = epoll_wait(epollSocket.GetSocketfd(), client_events.data(), max_epoll_event, -1);
        for (int i = 0; i < cnt; i++)
        {
            int fd = client_events[i].data.fd;

            if (client_events[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP))
            {
                std::cout << "[Worker " << worker_id << "] Client " << fd << " ngắt kết nối.\n";
                epoll_ctl(epollSocket.GetSocketfd(), EPOLL_CTL_DEL, fd, nullptr);
                CloseConnection(fd);
                current_conn--;
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
    close(fd);
    lru.remove(fd);
}

void Worker::ProcessHttpRequest(int fd, HttpRequest *request)
{
    Application::Router::Dispatch(fd, *request);
}

// Hiện đang kích ông cuối, cần FIX lại
void Worker::AddClient(int client_fd)
{
    if (lru.full())
    {
        int old_fd = lru.oldestKey();
        if (old_fd != -1)
        {
            epoll_ctl(epollSocket.GetSocketfd(), EPOLL_CTL_DEL, old_fd, nullptr);
            close(old_fd);
            lru.remove(old_fd);
            current_conn--;
        }
    }

    set_nonblocking(client_fd);

    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.data.fd = client_fd;

    if (epoll_ctl(epollSocket.GetSocketfd(), EPOLL_CTL_ADD, client_fd, &ev) < 0)
    {
        perror("epoll_ctl ADD client");
        close(client_fd);
        return;
    }

    lru.put(client_fd);
    current_conn++;
}

void Worker::HandleRequest(int fd)
{
    HttpRequest *request = lru.get(fd);
    if (!request)
        return;

    int capacity = static_cast<int>(request->cache.size());

    while (true)
    {
        int space = capacity - request->tail_idx;

        // Kiểm tra buffer đầy TRƯỚC khi gọi recv
        if (space <= 0)
        {
            // Buffer đầy nhưng chưa parse xong 1 request -> Request quá lớn
            SendStatusResponse(fd, 413, "Payload Too Large");
            CloseConnection(fd);
            return;
        }

        char *write_ptr = &request->cache[request->tail_idx];
        ssize_t bytes_read = recv(fd, write_ptr, static_cast<size_t>(space), 0);

        if (bytes_read < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Đã đọc cạn dữ liệu trong socket OS, thoát để đợi sự kiện epoll tiếp theo
                break;
            }
            if (errno == EINTR)
            {
                // Bị ngắt bởi tín hiệu (signal), đọc lại ngay lập tức
                continue;
            }
            // Lỗi socket khác -> Đóng kết nối
            CloseConnection(fd);
            return;
        }
        else if (bytes_read == 0)
        {
            // Client ngắt kết nối (EOF)
            CloseConnection(fd);
            return;
        }

        // 3. Nếu đọc thành công dữ liệu, cộng vào tail_idx
        request->tail_idx += static_cast<int>(bytes_read);

        // 4. Vòng lặp parse và xử lý request (hỗ trợ HTTP Pipelining)
        bool connection_closed = false;
        while (true)
        {
            const HttpParserState *state = request->Parse();

            if (state == CompleteState::Instance())
            {
                // A. Xử lý request đồng bộ và gửi phản hồi
                ProcessHttpRequest(fd, request);

                // B. Dịch chuyển phần thừa còn lại về đầu mảng
                int remain = request->tail_idx - request->curr_idx;
                if (remain > 0)
                {
                    std::memmove(&request->cache[0], &request->cache[request->curr_idx], remain);
                }

                request->NextRequest(remain);

                continue;
            }
            else if (state == ErrorState::Instance())
            {
                // Parse lỗi -> gửi 400 Bad Request và đóng kết nối
                std::string error_mgs = "Bad Request";
                SendStatusResponse(fd, 400, error_mgs);
                CloseConnection(fd);
                connection_closed = true;
                break;
            }
            else
            {

                break;
            }
        }

        if (connection_closed)
        {
            return;
        }
    }
}

int Worker::getConnCount()
{
    return current_conn.load();
}