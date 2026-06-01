#pragma once

#include "Common/LRU.h"
#include "Common/SocketGuard.h"
#include "Node/ClientDetails.h"
#include "Http/HttpParser.h"
#include "Network_Common.h"

template <typename T>
class Worker
{
private:
    int worker_id;
    int max_epoll_event;
    int max_client;
    SocketGuard epollSocket;
    std::vector<epoll_event> client_events;
    LRUCache<T> lru;
    std::unordered_map<int, T *> clients;

    std::atomic<int> current_conn{0};

public:
    Worker(int id, int max_events, int max_client);
    void AddClient(int client_fd);
    void StartWorker();
    void HandleRequest(T *detail);
    int getConnCount();
};
