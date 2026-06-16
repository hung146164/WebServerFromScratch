/*!
    \file Node.h
    \brief Node defination
    \author HungForre
    \date 6/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_COMMON_NODE_H
#define CPPSERVER_COMMON_NODE_H

#include <memory>
#include <ctime>

template <typename T>
struct Node
{
    T value;
    int pre = -1;
    int next = -1;
    int key = -1;
    time_t last_active_time = 0;

    void Reset()
    {
        pre = -1;
        next = -1;
        key = -1;
        last_active_time = 0;
        value.Reset();
    }
};
#endif
