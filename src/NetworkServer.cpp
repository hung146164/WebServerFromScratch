#include "header/NetworkServer.h"
NetworkServer::NetworkServer()
{
    serverSocket.SetSocketfd(socket(AF_INET, SOCK_STREAM, 0));
    if (serverSocket.GetSocketfd() < 0)
    {
        std::cerr << "Create socket failed!\n";
        return;
    }

    // 1. Khởi tạo 16 Workers
    for (int i = 0; i < NUMBER_OF_WORKER; ++i)
    {
        Worker<ClientDetail<ParseState>> *w = new Worker<ClientDetail<ParseState>>(i, MAX_EVENTS, CLIENT_PER_THREAD);
        worker_instances.push_back(w);
    }
    newClients.resize(MAX_EVENTS);
}

NetworkServer::~NetworkServer()
{
    for (auto &thread : threads)
    {
        if (thread.joinable())
            thread.join();
    }

    for (auto w : worker_instances)
    {
        delete w;
    }
}

void NetworkServer::SetupNetwork()
{
    if (serverSocket.GetSocketfd() < 0)
    {
        std::cerr << "Socket not initialized!\n";
        return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    if (setsockopt(serverSocket.GetSocketfd(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "Set socket REUSEADDR failed!\n";
        return;
    }

    if (bind(serverSocket.GetSocketfd(), (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Bind failed!\n";
        return;
    }

    if (listen(serverSocket.GetSocketfd(), MAX_LISTEN) < 0)
    {
        std::cerr << "Listen failed!\n";
        return;
    }

    epollSocket.SetSocketfd(epoll_create1(0));
    epoll_event ev;
    ev.data.fd = serverSocket.GetSocketfd();
    ev.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(epollSocket.GetSocketfd(), EPOLL_CTL_ADD, serverSocket.GetSocketfd(), &ev) < 0)
    {
        std::cerr << "Epoll setup error!\n";
    }
}

void NetworkServer::AcceptClient()
{
    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    while (true)
    {
        int client_fd = accept(serverSocket.GetSocketfd(), (sockaddr *)&clientAddr, &clientAddrLen);

        if (client_fd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            if (errno == EMFILE || errno == ENFILE)
            {
                std::cerr << "Warning: System FD limit reached!\n";
            }
            break;
        }

        // Phân bổ Round-Robin
        worker_instances[next_worker]->AddClient(client_fd);
        next_worker = (next_worker + 1) % NUMBER_OF_WORKER;
    }
}

void NetworkServer::Start()
{
    SetupNetwork();

    // Chạy luồng cho từng Worker
    for (int i = 0; i < NUMBER_OF_WORKER; ++i)
    {
        threads.emplace_back(&Worker<ClientDetail<ParseState>>::StartWorker, worker_instances[i]);
    }

    std::cout << "[Server] Listening on port " << PORT << " with " << NUMBER_OF_WORKER << " threads.\n";

    while (true)
    {
        int cnt = epoll_wait(epollSocket.GetSocketfd(), newClients.data(), MAX_EVENTS, -1);
        for (int i = 0; i < cnt; i++)
        {
            if (newClients[i].data.fd == serverSocket.GetSocketfd())
            {
                AcceptClient();
            }
        }
    }
}

void NetworkServer::Stop()
{
    // Cleanup
}
