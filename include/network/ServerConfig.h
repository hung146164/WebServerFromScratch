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
    // [STATIC] - Cấu hình tĩnh nạp lúc khởi động (Yêu cầu restart server để thay đổi)
    uint16_t port = 8081;
    int num_workers = std::thread::hardware_concurrency();
    int client_per_worker = 100;
    int max_epoll_events = 50;
    int max_listen_queue = 50;
    bool enable_ebpf = false;

    // [DYNAMIC] - Cấu hình động nạp tức thì qua tín hiệu SIGHUP
    int max_client_per_ip = 2;
    int read_timeout_sec = 15;
};

#endif