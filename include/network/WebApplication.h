#pragma once
#include <memory>
class WebApplication
{
private:
    Router router;
    std::unique_ptr<NetworkServer> server;

public:
    WebApplication() = default;

    void Get(const std::string &path, HandlerFunc handler)
    {
        router.AddRoute("GET", path, handler);
    }

    void Post(const std::string &path, HandlerFunc handler)
    {
        router.AddRoute("POST", path, handler);
    }

    void Put(const std::string &path, HandlerFunc handler)
    {
        router.AddRoute("PUT", path, handler);
    }

    void Delete(const std::string &path, HandlerFunc handler)
    {
        router.AddRoute("DELETE", path, handler);
    }

    void Run(int port);
};
