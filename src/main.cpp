/*!
    \file main.cpp
    \brief Demo - English Center Management with JSON file persistence
    \author HungForre
    \date 7/6/2026
    \copyright VDT
*/

#include "network/NetworkServer.h"
#include "network/ServerConfig.h"
#include "server/http/Router.h"
#include "server/http/HttpResponse.h"
#include "server/http/HttpStatic.h"
#include "server/http/HttpUtils.h"
#include "server/http/HttpMethod.h"

#include <fstream>
#include <sstream>
#include <mutex>
#include <filesystem>
#include <cstdio>

int main()
{
    ServerConfig cfg;
    cfg.port = 8081;
    cfg.num_workers = 2;

    NetworkServer server(cfg);
    server.Start();
    return 0;
}
