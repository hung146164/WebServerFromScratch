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
#include <sys/sendfile.h>

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

    /*
        Chờ sự kiện mạng, timeout 500ms.
        Nếu không có gì trong 500ms thì thoát để chạy timeout check bên dưới.
    */
    time_t last_timeout_check = 0;
    while (is_running)
    {
        int cnt = epoll_wait(epoll_socket.GetSocketfd(),
                             client_events.data(),
                             config.max_epoll_events,
                             500); // 500ms timeout

        /*
            Nếu bị ngắt bởi signal (EINTR) thì thử lại. Lỗi thật thì thoát vòng lặp.
        */
        if (cnt < 0)
        {
            if (errno == EINTR)
                continue;
            std::cerr << "[Worker " << worker_id << "] epoll_wait error\n";
            break;
        }

        time_t now = time(nullptr);
        if (cnt > 0)
        {
            for (int i = 0; i < cnt; ++i)
            {
                int fd = client_events[i].data.fd;
                uint32_t events = client_events[i].events;

                /*
                    Client ngắt kết nối (RDHUP),
                    kết nối bị treo (HUP), hoặc
                    lỗi socket (ERR)
                */
                if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
                {
                    CloseConnection(fd);
                    continue;
                }

                // Nếu có client mới thì accept
                if (fd == server_socket.GetSocketfd())
                {
                    AcceptClient();
                }
                else
                {
                    // Client có cờ này tức là đang download file
                    if (events & EPOLLOUT)
                    {
                        HttpRequest *req = lru.GetWithoutMove(fd);
                        if (req && req->is_sending_file)
                        {
                            ContinueSendFile(fd, req);
                            continue;
                        }
                    }

                    // Có dữ liệu đến từ client, đọc và xử lý HTTP request.
                    if (events & EPOLLIN)
                    {
                        HandleRequest(fd);
                    }
                }
            }
        }

        /*
            Mỗi giây sẽ quét LRU một lần kích những
            connection không gửi nhận gì quá 1s
            vì LRU đã sắp xếp theo cũ nhất nên ko lo kick nhầm

        */
        if (now - last_timeout_check >= 1)
        {
            last_timeout_check = now;
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

    /*
        Mặc định khi close() thông thường: Hệ điều hành sẽ cố gắng gửi
        nốt số dữ liệu còn kẹt trong buffer mạng (từ lệnh sendfile trước đó)
        rồi mới đóng

        Khi cấu hình l_onoff = 1 và l_linger = 0, Sẽ yêu cầu bỏ qua quy trình
        đóng thông thường, hủy liên kết ngay lập tức
        Socket bị đóng ngay mà không đi qua trạng thái chờ TIME_WAIT của TCP
    */
    struct linger sl;
    sl.l_onoff = 1;
    sl.l_linger = 0;
    setsockopt(fd, SOL_SOCKET, SO_LINGER, &sl, sizeof(sl));

    shutdown(fd, SHUT_WR);
    char buf[128];

    /*
        Đọc hết dữ liệu còn tồn đọng trong receive buffer
        để tránh kernel gửi RST không cần thiết.
    */
    while (recv(fd, buf, sizeof(buf), 0) > 0)
    {
    }

    close(fd);

    HttpRequest *req = lru.GetWithoutMove(fd);
    if (req && !req->client_ip.empty())
    {
        ip_connections[req->client_ip]--;
        if (ip_connections[req->client_ip] <= 0)
        {
            ip_connections.erase(req->client_ip);
        }
    }

    lru.Remove(fd);
}

void Worker::AcceptClient()
{
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    while (true)
    {
        /*
            accept4 non-blocking, là một hàm nâng cao của accept
            giúp có thể thiết lập cờ đồng thời.
        */
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

        // Lấy ip
        char ip_str[INET_ADDRSTRLEN];
        std::string client_ip = "0.0.0.0";
        if (inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN))
        {
            client_ip = ip_str;
        }

        if (ip_connections[client_ip] >= config.max_client_per_ip)
        {
            std::string limit_msg = "[IP Limit] Rejecting client from " + client_ip + " on Worker " + std::to_string(worker_id) + " (Limit " + std::to_string(config.max_client_per_ip) + " reached)\n";
            std::cout << limit_msg;
            close(client_fd);
            continue;
        }

        ip_connections[client_ip]++;

        if (lru.Full())
        {
            int old_fd = lru.OldestKey();
            if (old_fd != -1)
            {
                CloseConnection(old_fd);
            }
        }

        /*Thiết lập 3 cờ
        EPOLLIN(Đọc),
        EPOLLET(Edge-Triggered),
        EPOLLRDHUP(client đóng kết nối)
        */
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.fd = client_fd;

        if (epoll_ctl(epoll_socket.GetSocketfd(), EPOLL_CTL_ADD, client_fd, &ev) < 0)
        {
            std::cerr << "[Worker " << worker_id << "] epoll_ctl ADD failed\n";
            close(client_fd);
            ip_connections[client_ip]--;
            continue;
        }

        lru.Put(client_fd);
        HttpRequest *req = lru.GetWithoutMove(client_fd);
        if (req)
        {
            req->client_ip = client_ip;
        }
    }
}

void Worker::ProcessHttpRequest(int fd, HttpRequest *request)
{
    // Đinh tuyến đến handler đăng kí
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
        if (request->is_sending_file)
            break;
        /*
            Kiểm tra nếu payload hiện tại cbi vượt qua X kb của cache thì bỏ
            cache hiện tại đang size max là 64kb
        */
        int space = capacity - request->tail_idx;
        if (space <= 0)
        {
            std::string error_msg = "Payload Too Large";
            Http::Error(fd, 413, error_msg);
            CloseConnection(fd);
            return;
        }

        // lấy con trỏ kí tự cuối ở cache, thêm text vào
        char *write_ptr = &request->cache[request->tail_idx];
        ssize_t bytes_read = recv(fd, write_ptr, (int)space, 0);

        if (bytes_read < 0)
        {

            /*
                EAGAIN / EWOULDBLOCK: socket non-blocking,
                không có dữ liệu nào trong buffer của kernel lúc này ,
                chỉ là "chưa có gì, thử lại sau". break thoát vòng ngoài,
                chờ EPOLLIN event tiếp theo.
            */
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;

            /*
                bị ngắt quãng bởi signal
            */
            if (errno == EINTR)
                continue;

            // lỗi thực sự
            CloseConnection(fd);
            return;
        }
        /*
            tín hiệu báo đã đóng connection
        */
        else if (bytes_read == 0)
        {
            CloseConnection(fd);
            return;
        }

        request->tail_idx += (int)bytes_read;

        bool connection_closed = false;
        while (true)
        {
            /*
                Vòng lặp để xử lý HTTP pipelining, 1 lần recv() có thể chứa nhiều request
                nối đuôi nhau trong cùng TCP packet.
                Parse() chạy state machine qua buffer từ curr_idx đến tail_idx,
                trả về trạng thái hiện tại.
            */
            const HttpParserState *state = request->Parse();

            if (state == CompleteState::Instance())
            {
                // Khi đã Parse xong xử lý request
                ProcessHttpRequest(fd, request);

                /*Nếu request là tải file thì bỏ Epollin(đọc) để xử lý tải xuống
                Nếu ở cuối request 1 có thừa một chút của request 2 thì cắt cho lên luôn
                */

                if (request->is_sending_file)
                {
                    epoll_event ev{};
                    ev.events = EPOLLOUT | EPOLLET | EPOLLRDHUP;
                    ev.data.fd = fd;
                    epoll_ctl(epoll_socket.GetSocketfd(), EPOLL_CTL_MOD, fd, &ev);

                    int remain = request->tail_idx - request->curr_idx;
                    if (remain > 0)
                        std::memmove(&request->cache[0],
                                     &request->cache[request->curr_idx],
                                     (int)remain);
                    request->NextRequest(remain);
                    lru.MakeRecent(fd);

                    ContinueSendFile(fd, request);

                    connection_closed = true;
                    break;
                }
                // request 1 đã xử lý xong cắt request 2 lên đầu tiên
                // Reset các con trỏ duyệt về 0.
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

void Worker::ContinueSendFile(int fd, HttpRequest *req)
{
    time_t now = time(nullptr);

    while (req->file_remaining > 0)
    {
        ssize_t sent = sendfile(fd, req->file_fd, &req->file_offset, (size_t)req->file_remaining);

        if (sent > 0)
        {
            req->file_remaining -= sent;
            req->bytes_sent_in_period += sent;
            lru.MakeRecent(fd);
        }
        else if (sent < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            CloseConnection(fd);
            return;
        }
        else
        {
            CloseConnection(fd);
            return;
        }
    }

    /*
        Nếu thời gian tải đã lớn hơn 5s mà tốc độ tải của client< 5kb
        thì sẽ kick luôn.
    */
    time_t elapsed = now - req->last_speed_check_time;
    if (elapsed >= 5)
    {
        double speed = (double)req->bytes_sent_in_period / elapsed;
        if (speed < 5120.0)
        {
            std::cout << "[Slow Connection] Kicking client fd " << fd
                      << " due to low speed: " << (speed / 1024.0) << " KB/s\n";
            CloseConnection(fd);
            return;
        }
        req->last_speed_check_time = now;
        req->bytes_sent_in_period = 0;
    }
    // Nếu gửi xong thì bật lại EpollIn ,bỏ epollout, đóng file
    if (req->file_remaining == 0)
    {
        close(req->file_fd);
        req->file_fd = -1;
        req->is_sending_file = false;

        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.fd = fd;
        epoll_ctl(epoll_socket.GetSocketfd(), EPOLL_CTL_MOD, fd, &ev);
    }
    /*Nếu chưa xong hàng đợi dữ liệu ở client đang đầy chưa thể gửi thêm
    thì sẽ gửi ở sự kiên epoll out tiếp theo khi hàng đợi dữ liệu của client
    có thể gửi
    */
}
