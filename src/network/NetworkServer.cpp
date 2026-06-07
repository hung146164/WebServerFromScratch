/*!
    \file NetworkServer.cpp
    \brief NetworkServer implementation
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/
#include "network/NetworkServer.h"

#include <iostream>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

NetworkServer::NetworkServer(ServerConfig config_)
    : config(config_)
{
    serverSocket.SetSocketfd(socket(AF_INET, SOCK_STREAM, 0));
    if (serverSocket.GetSocketfd() < 0)
    {
        std::cerr << "[Server] Create socket failed!\n";
        return;
    }

    newClients.resize((size_t)config_.max_epoll_events);

    SetupNetwork();
}

void NetworkServer::SetupThread()
{
    for (int i = 0; i < config.num_workers; ++i)
        worker_instances.push_back(new Worker(i, config));

    for (int i = 0; i < config.num_workers; ++i)
        threads.emplace_back(&Worker::StartWorker, worker_instances[i]);
}

NetworkServer::~NetworkServer()
{
    for (auto &t : threads)
        if (t.joinable())
            t.join();

    for (auto w : worker_instances)
        delete w;
}

void NetworkServer::SetupNetwork()
{
    if (serverSocket.GetSocketfd() < 0)
    {
        std::cerr << "[Server] Socket not initialized!\n";
        return;
    }

    int opt = 1;
    if (setsockopt(serverSocket.GetSocketfd(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "[Server] Set socket REUSEADDR failed!\n";
        return;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)config.port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket.GetSocketfd(), (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "[Server] Bind failed!\n";
        return;
    }

    if (listen(serverSocket.GetSocketfd(), config.max_listen_queue) < 0)
    {
        std::cerr << "[Server] Listen failed!\n";
        return;
    }

    epollSocket.SetSocketfd(epoll_create1(0));
    if (epollSocket.GetSocketfd() < 0)
    {
        std::cerr << "[Server] epoll_create1 failed!\n";
        return;
    }

    epoll_event ev{};
    ev.data.fd = serverSocket.GetSocketfd();
    ev.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(epollSocket.GetSocketfd(), EPOLL_CTL_ADD, serverSocket.GetSocketfd(), &ev) < 0)
        std::cerr << "[Server] Epoll setup error!\n";
}

void NetworkServer::AcceptClient()
{
    sockaddr_in clientAddr{};
    socklen_t clientAddrLen = sizeof(clientAddr);

    while (true)
    {
        int client_fd = accept(serverSocket.GetSocketfd(),
                               (sockaddr *)&clientAddr,
                               &clientAddrLen);
        if (client_fd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            if (errno == EMFILE || errno == ENFILE)
                std::cerr << "[Server] Warning: FD limit reached!\n";
            break;
        }

        worker_instances[next_worker]->AddClient(client_fd);
        next_worker = (next_worker + 1) % config.num_workers;
    }
}

void NetworkServer::Start()
{
    SetupThread();
    std::cout << "[Server] Listening on port " << config.port
              << " with " << config.num_workers << " worker threads.\n";

    while (true)
    {
        int cnt = epoll_wait(epollSocket.GetSocketfd(),
                             newClients.data(),
                             config.max_epoll_events,
                             -1);
        for (int i = 0; i < cnt; ++i)
        {
            if (newClients[i].data.fd == serverSocket.GetSocketfd())
                AcceptClient();
        }
    }
}

void NetworkServer::Stop()
{
    // TODO: eventfd wakeup
}
