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
#include <memory>

#include "common/SocketGuard.h"
#include "network/Worker.h"
#include "network/ServerConfig.h"

class NetworkServer
{
private:
    ServerConfig config;

    std::vector<std::unique_ptr<Worker>> worker_instances;
    std::vector<std::thread> threads;
    std::atomic<bool> is_running{false};

    int next_worker{0};

    int wakeup_fd{-1};

    void SetupThread();

public:
    explicit NetworkServer(ServerConfig config_);
    ~NetworkServer();
    void Start();
    void Stop();
};

#endif