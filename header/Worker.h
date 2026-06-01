#pragma once

#include "Common/LRUClient.h"
#include "Common/SocketGuard.h"
#include "Node/ClientDetails.h"
#include "Http/HttpParser.h"
#include "Network_Common.h"

class Worker
{
private:
    int worker_id;
    int max_epoll_event;
    int max_client;
    SocketGuard epollSocket;
    std::vector<epoll_event> client_events;
    LRUClient lru;

    std::atomic<int> current_conn{0};

public:
    Worker(int id, int max_events, int max_client);
    void AddClient(int client_fd);
    void StartWorker();
    void HandleRequest(HttpRequest *request);
    int getConnCount();
};
