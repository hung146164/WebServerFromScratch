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

#include "common/LRU.h"
#include "common/SocketGuard.h"
#include "server/http/HttpRequest.h"
#include "server/http/Router.h"
#include "network/ServerConfig.h"

class Worker
{
private:
    int worker_id;
    ServerConfig config;

    SocketGuard epollSocket;
    std::vector<epoll_event> client_events;

    LRUClient<HttpRequest> lru;

    std::atomic<int> current_conn{0};

    void SetupWorker();
    void HandleRequest(int fd);
    void CloseConnection(int fd);
    void ProcessHttpRequest(int fd, HttpRequest *request);
    void SendStatusResponse(int fd, int status_code, std::string_view msg);

public:
    Worker(int worker_id_, ServerConfig config_);

    void StartWorker();
    void AddClient(int client_fd);
    int getConnCount();
};

#endif