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
#include <sys/epoll.h>
#include <string_view>

#include "common/LRU.h"
#include "server/http/HttpRequest.h"
#include "common/SocketGuard.h"

class Worker
{
private:
    int worker_id;
    int max_epoll_event;
    int max_client;

    SocketGuard epollSocket;
    std::vector<epoll_event> client_events;

    LRUClient<HttpRequest> lru;

    std::atomic<int> current_conn{0};

private:
    void StartWorker();

    void HandleRequest(int fd);

    void CloseConnection(int fd);

    void ProcessHttpRequest(int fd, HttpRequest *request);

    void SendStatusResponse(int fd, int errcode, std::string_view mgs);

public:
    Worker(int worker_id_, int max_epoll_event_, int max_client_);

    void AddClient(int client_fd);
    int getConnCount();
};

#endif