/*!
    \file Node.h
    \brief Node doubly-linked list cho LRU
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
    Node *pre  = nullptr;
    Node *next = nullptr;
    int   key  = -1;          // key = socket fd (int), -1 la sentinel
    std::unique_ptr<T> value;

    Node() { value = std::make_unique<T>(); }

    void Reset()
    {
        pre  = nullptr;
        next = nullptr;
        key  = -1;
        if (value)
            value->Reset();
    }
};
#endif
