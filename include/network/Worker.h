/*!
    \file Worker.h
    \brief Worker class definition
    \author HungForre
    \date 6/6/2026
    \copyright VDT
*/

#ifndef CPPSERVER_NETWORK_WORKER_H
#define CPPSERVER_NETWORK_WORKER_H

#include <vector>
#include <atomic>
#include <string>
#include <string_view>
#include <sys/epoll.h>

#include "common/LRUCustom.h"
#include "common/SocketGuard.h"
#include "server/http/HttpRequest.h"
#include "server/http/Router.h"
#include "network/ServerConfig.h"

class Worker
{
private:
    int worker_id;
    ServerConfig config;

    SocketGuard server_socket;
    SocketGuard epoll_socket;

    std::vector<epoll_event> client_events;

    LRUCustom<HttpRequest> lru;

    void SetupWorker();
    void SetupNetwork();

    void AcceptClient();
    void HandleRequest(int fd);
    void CloseConnection(int fd);
    void ProcessHttpRequest(int fd, HttpRequest *request);

public:
    Worker(int worker_id_, ServerConfig config_);

    void StartWorker();
};

#endif