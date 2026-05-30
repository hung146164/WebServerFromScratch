#include "Network_Common.h"

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
        fd = fd;
    }
    int GetSocketfd()
    {
        return fd;
    }
};