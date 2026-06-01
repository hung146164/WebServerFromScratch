#pragma once

#include "CppConfig.h"

class SocketGuard
{
private:
    int fd = -1;

public:
    ~SocketGuard()
    {
        if (fd != -1)
        {
            close(fd);
        }
    }
    void SetSocketfd(int fd_)
    {
        fd = fd_;
    }
    int GetSocketfd()
    {
        return fd;
    }
};