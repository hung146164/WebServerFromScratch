#pragma once

#include "CppConfig.h"
#include "Common/SocketGuard.h"
#include "Worker.h"
#include "Node/ClientDetails.h"
#include "Http/HttpParser.h"
#include "ServerConfig.h"

class NetworkServer
{

private:
    SocketGuard serverSocket;
    SocketGuard epollSocket;
    std::vector<Worker *> worker_instances;
    std::vector<epoll_event> newClients;
    std::vector<std::thread> threads;
    int next_worker{0};

    void SetupNetwork();
    void AcceptClient();

public:
    NetworkServer();
    ~NetworkServer();
    void Start();
    void Stop();
};
