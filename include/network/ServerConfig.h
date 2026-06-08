/*!
    \file ServerConfig.h
    \brief serverfig
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_NETWORK_SERVERCONFIG_H
#define CPPSERVER_NETWORK_SERVERCONFIG_H

#include <thread>
#include <cstdint>

struct ServerConfig
{
    uint16_t port = 8080;
    uint32_t num_workers = (uint32_t)std::thread::hardware_concurrency();
    uint32_t client_per_worker = 5000;
    int max_epoll_events = 1024;
    int max_listen_queue = 1024;
    uint32_t max_client_per_ip = 10;
    uint32_t read_timeout_sec = 15;
};

#endif // CPPSERVER_NETWORK_SERVERCONFIG_H