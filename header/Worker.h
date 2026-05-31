#include "Network_Common.h"
#include "SocketGuard.h"
#include "ParseHttp.h"
#include "LRU.h"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <atomic>

class Worker
{
private:
    int worker_id;
    int max_epoll_event;
    int max_client;
    SocketGuard epollSocket;
    std::vector<epoll_event> client_events;
    LRUCache lru;

    // Biến đếm số lượng kết nối hiện tại của worker này
    std::atomic<int> current_conn{0};

public:
    Worker(int id, int max_events, int max_c)
        : worker_id(id), max_epoll_event(max_events), max_client(max_c), lru(max_c)
    {

        epollSocket.SetSocketfd(epoll_create1(0));
        client_events.resize(max_epoll_event);
        std::cout << "[Worker " << worker_id << "] Khởi tạo thành công.\n";
    }

    // Hàm này được luồng Main gọi để phân phối Client
    void AddClient(int client_fd)
    {
        // 1. Nếu LRU đầy, chủ động đóng kết nối cũ nhất trước khi nhận mới
        if (lru.cnt >= max_client)
        {
            int old_fd = lru.tail->key; // Lấy FD cũ nhất từ đuôi LRU
            std::cout << "[Worker " << worker_id << "] LRU đầy, đóng FD cũ: " << old_fd << "\n";

            // Xóa khỏi epoll và đóng socket
            epoll_ctl(epollSocket.GetSocketfd(), EPOLL_CTL_DEL, old_fd, nullptr);
            close(old_fd);

            // Xóa khỏi cấu trúc dữ liệu LRU (put mới sẽ tự ghi đè nếu bạn muốn,
            // nhưng ở đây ta thực hiện đóng vật lý)
        }

        // 2. Thiết lập Non-blocking cho client mới (rất quan trọng cho EPOLLET)
        set_nonblocking(client_fd);

        // 3. Đăng ký vào epoll của worker này
        epoll_event ev;
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.fd = client_fd;

        if (epoll_ctl(epollSocket.GetSocketfd(), EPOLL_CTL_ADD, client_fd, &ev) < 0)
        {
            perror("epoll_ctl ADD client");
            close(client_fd);
            return;
        }

        // 4. Lưu vào LRU (Key: fd, Value: 1 - đánh dấu đang hoạt động)
        lru.put(client_fd, 1);
        current_conn++;
    }

    void StartWorker()
    {
        std::cout << "[Worker " << worker_id << "] Bắt đầu vòng lặp xử lý.\n";
        while (true)
        {
            int cnt = epoll_wait(epollSocket.GetSocketfd(), client_events.data(), max_epoll_event, -1);
            for (int i = 0; i < cnt; i++)
            {
                int fd = client_events[i].data.fd;

                // Nếu client ngắt kết nối hoặc có lỗi
                if (client_events[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP))
                {
                    std::cout << "[Worker " << worker_id << "] Client " << fd << " ngắt kết nối.\n";
                    epoll_ctl(epollSocket.GetSocketfd(), EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);
                    current_conn--;
                    // (Tùy chọn: Xóa khỏi LRU hoặc để nó tự bị đẩy ra sau)
                }
                else
                {
                    // Cập nhật LRU: Đưa FD vừa có dữ liệu lên đầu (MRU)
                    lru.get(fd);
                    HandleRequest(fd);
                }
            }
        }
    }

    void HandleRequest(int fd)
    {
        // Logic parse HTTP và phản hồi của bạn ở đây
        // Sau khi phản hồi xong, nếu không dùng Keep-alive thì close(fd)
    }

    int getConnCount() { return current_conn.load(); }
};