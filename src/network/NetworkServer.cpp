#include "network/NetworkServer.h"
#include "common/Json/JsonParser.h"
#include "common/Json/JsonDocument.h"
#include "server/http/Router.h"

#include <iostream>
#include <cerrno>
#include <sys/socket.h>
#include <fstream>
#include <sstream>
#include <string_view>

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

        int old_port = config.port;
        int old_workers = config.num_workers;

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
        if (doc["enable_ebpf"].GetNode()->type == Json::JsonType::BOOL)
        {
            std::string_view ebpf_str = doc["enable_ebpf"].ToString();

            if (ebpf_str == "true")
            {
                config.enable_ebpf = true;
            }
            else if (ebpf_str == "false")
            {
                config.enable_ebpf = false;
            }
            else
            {
                throw std::runtime_error("Invalid boolean value for enable_ebpf");
            }
        }

        for (auto &worker : worker_instances)
        {
            if (worker)
            {
                worker->UpdateConfig(config);
            }
        }

        if (!is_running)
        {
            std::cout << "[Config] Successfully loaded initial config.json:\n"
                      << "         Port: " << config.port << "\n"
                      << "         Workers: " << config.num_workers << "\n"
                      << "         Read Timeout: " << config.read_timeout_sec << "s\n"
                      << "         Max Client Per IP: " << config.max_client_per_ip << "\n"
                      << "         Rate Limit: " << Http::Router::rate_limit_per_sec << " req/sec\n";
        }
        else
        {
            std::cout << "[Config] Configuration hot-reloaded via SIGHUP:\n"
                      << "         [DYNAMIC] Read Timeout: " << config.read_timeout_sec << "s (Applied)\n"
                      << "         [DYNAMIC] Max Client Per IP: " << config.max_client_per_ip << " (Applied)\n"
                      << "         [DYNAMIC] Rate Limit: " << Http::Router::rate_limit_per_sec << " req/sec (Applied)\n";

            if (config.port != old_port || config.num_workers != old_workers)
            {
                std::cout << "         [WARNING] Static settings changed in config.json:\n";
                if (config.port != old_port)
                {
                    std::cout << "                   - Port changed from " << old_port << " to " << config.port << "\n";
                    config.port = old_port;
                }
                if (config.num_workers != old_workers)
                {
                    std::cout << "                   - Workers changed from " << old_workers << " to " << config.num_workers << "\n";
                    config.num_workers = old_workers;
                }
                std::cout << "                   (These static changes require server restart to apply!)\n";
            }
        }
    }
    catch (...)
    {
        std::cout << "[Config] Error parsing config.json. Keeping existing configuration.\n";
    }
}
