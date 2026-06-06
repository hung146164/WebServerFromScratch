/*!
    \file SocketGuard.h
    \brief SocketGuard RAII wrapper
    \author HungForre
    \date 6/6/2026
    \copyright VDT
*/

#ifndef CPPSERVER_COMMON_SOCKETGUARD_H
#define CPPSERVER_COMMON_SOCKETGUARD_H

#include <unistd.h>
#include <utility>

class SocketGuard
{
private:
    int fd = -1;

public:
    SocketGuard() = default;

    explicit SocketGuard(int fd_) : fd(fd_) {}

    ~SocketGuard()
    {
        Close();
    }

    SocketGuard(const SocketGuard &) = delete;
    SocketGuard &operator=(const SocketGuard &) = delete;

    SocketGuard(SocketGuard &&other) noexcept : fd(other.fd)
    {
        other.fd = -1;
    }

    SocketGuard &operator=(SocketGuard &&other) noexcept
    {
        if (this != &other)
        {
            Close();
            fd = other.fd;
            other.fd = -1;
        }
        return *this;
    }

    void SetSocketfd(int fd_)
    {
        Close();
        fd = fd_;
    }

    int GetSocketfd() const
    {
        return fd;
    }

    void Close()
    {
        if (fd != -1)
        {
            close(fd);
            fd = -1;
        }
    }

    int Release()
    {
        int temp = fd;
        fd = -1;
        return temp;
    }
};

#endif