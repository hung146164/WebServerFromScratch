/*!
    \file NetworkServer.h
    \brief NetworkServer definition
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/

#ifndef CPPSERVER_NETWORK_NETWORKSERVER_H
#define CPPSERVER_NETWORK_NETWORKSERVER_H

#include <vector>
#include <thread>
#include <sys/epoll.h>

#include "common/SocketGuard.h"
#include "network/Worker.h"
#include "network/ServerConfig.h"

class NetworkServer
{
private:
    ServerConfig config;

    SocketGuard serverSocket;
    SocketGuard epollSocket;
    std::vector<Worker *> worker_instances;
    std::vector<epoll_event> newClients;
    std::vector<std::thread> threads;
    std::atomic<bool> is_running{false};

    int next_worker{0};

    int wakeup_fd{-1};

    void SetupNetwork();
    void SetupThread();
    void AcceptClient();

public:
    explicit NetworkServer(ServerConfig config_ = ServerConfig{});
    ~NetworkServer();
    void Start();
    void Stop();
};

#endif