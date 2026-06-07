/*!
    \file ServerConfig.h
    \brief ServerConfig
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/

#ifndef CPPSERVER_NETWORK_SERVERCONFIG_H
#define CPPSERVER_NETWORK_SERVERCONFIG_H

#include <thread>

struct ServerConfig
{
    int port = 8080;
    int num_workers = static_cast<int>(std::thread::hardware_concurrency());
    int client_per_worker = 5000;
    int max_epoll_events = 1024;
    int max_listen_queue = 1024;
    int max_client_per_ip = 10;
    int read_timeout_sec = 15;
};

#endif