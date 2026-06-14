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

template <typename T>
struct Node
{
    T value;
    int pre = -1;
    int next = -1;
    int key = -1;

    void Reset()
    {
        pre = -1;
        next = -1;
        key = -1;
        value.Reset();
    }
};
#endif
