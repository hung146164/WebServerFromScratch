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

NetworkServer::NetworkServer(ServerConfig config_) : config(config_)
{
}

void NetworkServer::SetupThread()
{
    for (int i = 0; i < config.num_workers; ++i)
    {
        worker_instances.push_back(std::make_unique<Worker>(i, config));
    }

    for (int i = 0; i < config.num_workers; ++i)
        threads.emplace_back(&Worker::StartWorker, worker_instances[i].get());
}

NetworkServer::~NetworkServer()
{
    for (auto &t : threads)
        if (t.joinable())
            t.join();
}

void NetworkServer::Start()
{
    SetupThread();

    while (true)
    {
        pause();
    }
}

void NetworkServer::Stop()
{
    // TODO: eventfd wakeup
}
