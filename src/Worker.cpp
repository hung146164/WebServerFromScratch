#include "Worker.h"

Worker::Worker(int id, int max_events, int max_client)
    : worker_id(id), max_epoll_event(max_events), max_client(max_client), lru(max_client)
{
    epollSocket.SetSocketfd(epoll_create1(0));
    client_events.resize(max_epoll_event);
}

void Worker::AddClient(int client_fd)
{
    if (lru.full())
    {
        // Cân nhắc nên bỏ ông cuối hay bỏ ông mới đến

        // Bỏ ông mới đến

        // bỏ ông cuối
        int old_fd = lru.oldestKey();
        if (old_fd != -1)
        {

            epoll_ctl(epollSocket.GetSocketfd(), EPOLL_CTL_DEL, old_fd, nullptr);
            close(old_fd);
            lru.remove(old_fd);
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

void Worker::StartWorker()
{

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
                close(fd);
                lru.remove(fd);
                current_conn--;
            }
            else
            {
                // HandleRequest(lru.get(fd));
                Test(fd);
            }
        }
    }
}

// void Worker::HandleRequest(HttpRequest *request)
// {

// }

// Test
char global_test_buffer[8192];

void Worker::Test(int fd)
{
    while (true)
    {
        ssize_t bytes_received = recv(fd, global_test_buffer, sizeof(global_test_buffer), 0);

        if (bytes_received > 0)
        {
            const char *response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 13\r\n"
                "Connection: keep-alive\r\n"
                "\r\n"
                "Hello Viettel!";
            send(fd, response, strlen(response), 0);
            // Với HTTP/1.1 đơn giản, đọc xong 1 lần có thể thoát nếu bạn chắc chắn client chỉ gửi 1 request/packet
        }
        else if (bytes_received == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break; // Đã đọc hết dữ liệu hiện có trong buffer OS
            }
            // Lỗi thực sự
            return;
        }
        else
        {
            // Client đóng kết nối
            return;
        }
    }
}

int Worker::getConnCount()
{
    return current_conn.load();
}
