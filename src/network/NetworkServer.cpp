#include "network/NetworkServer.h"
#include "common/Json/JsonParser.h"
#include "common/Json/JsonDocument.h"
#include "server/http/Router.h"

#include <iostream>
#include <cerrno>
#include <sys/socket.h>
#include <fstream>
#include <sstream>

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
    ReloadConfig();

    is_running = true;
    SetupThread();

    while (is_running)
    {
        pause();
    }
    std::cout << "[Server] Stopping network server...\n";
}

void NetworkServer::Stop()
{
    is_running = false;
    for (auto &worker : worker_instances)
    {
        if (worker)
        {
            worker->StopWorker();
        }
    }
}

void NetworkServer::ReloadConfig()
{
    std::cout << "[Config] Reading configuration file config.json...\n";
    std::ifstream file("config.json");
    if (!file.is_open())
    {
        std::cout << "[Config] config.json not found, using default configuration.\n";
        return;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    try
    {
        Json::JsonParser parser(100);
        Json::JsonDocument doc = parser.Parse(content);

        if (doc["port"].GetNode())
            config.port = std::stoi(doc["port"].ToString());
        if (doc["num_workers"].GetNode())
            config.num_workers = std::stoi(doc["num_workers"].ToString());
        if (doc["client_per_worker"].GetNode())
            config.client_per_worker = std::stoi(doc["client_per_worker"].ToString());
        if (doc["max_epoll_events"].GetNode())
            config.max_epoll_events = std::stoi(doc["max_epoll_events"].ToString());
        if (doc["max_listen_queue"].GetNode())
            config.max_listen_queue = std::stoi(doc["max_listen_queue"].ToString());
        if (doc["max_client_per_ip"].GetNode())
            config.max_client_per_ip = std::stoi(doc["max_client_per_ip"].ToString());
        if (doc["read_timeout_sec"].GetNode())
            config.read_timeout_sec = std::stoi(doc["read_timeout_sec"].ToString());
        if (doc["rate_limit_per_sec"].GetNode())
            Http::Router::rate_limit_per_sec = std::stoi(doc["rate_limit_per_sec"].ToString());

        std::cout << "[Config] Successfully loaded config.json using custom JsonParser:\n"
                  << "         Port: " << config.port << "\n"
                  << "         Workers: " << config.num_workers << "\n"
                  << "         Read Timeout: " << config.read_timeout_sec << "s\n"
                  << "         Max Client Per IP: " << config.max_client_per_ip << "\n"
                  << "         Rate Limit: " << Http::Router::rate_limit_per_sec << " req/sec\n";
    }
    catch (...)
    {
        std::cout << "[Config] Error parsing config.json. Keeping existing configuration.\n";
    }
}
