#include "WebApplication.h"

void WebApplication::Run(int port)
{
    server = std::make_unique<NetworkServer>(port, &router);
    server->Start();
}
