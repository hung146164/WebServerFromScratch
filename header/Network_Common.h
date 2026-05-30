#pragma once

#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <sys/epoll.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <map>
#include <numeric>
#include <algorithm>
#include <cmath>

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        std::cerr << "fcntl F_GETFL" << '\n';

        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        std::cerr << "fcntl F_SETFL O_NONBLOCK" << '\n';
    }
}