#include "header/Worker.h"

template <typename T>
Worker<T>::Worker(int id, int max_events, int max_client)
    : worker_id(id), max_epoll_event(max_events), max_client(max_client), lru(max_client)
{
    epollSocket.SetSocketfd(epoll_create1(0));
    client_events.resize(max_epoll_event);
    std::cout << "[Worker " << worker_id << "] Khởi tạo thành công.\n";
}

template <typename T>
void Worker<T>::AddClient(int client_fd)
{
    if (lru.full())
    {
        int old_fd = lru.oldestKey();
        if (old_fd != -1)
        {
            std::cout << "[Worker " << worker_id << "] LRU đầy, đóng FD cũ: " << old_fd << "\n";
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

    lru.put(client_fd, new T());
    current_conn++;
}

template <typename T>
void Worker<T>::StartWorker()
{
    std::cout << "[Worker " << worker_id << "] Bắt đầu vòng lặp xử lý.\n";
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
                HandleRequest(lru.get(fd));
            }
        }
    }
}

template <typename T>
void Worker<T>::HandleRequest(T *detail)
{
}

template <typename T>
int Worker<T>::getConnCount()
{
    return current_conn.load();
}

// Explicit instantiation for the current usage in the project.
template class Worker<ClientDetail<ParseState>>;
