/*!
    \file Node.h
    \brief Node
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
    Node *pre{};
    Node *next{};
    int key = -1;
    std::unique_ptr<T> value;

    Node()
    {
        value = std::make_unique<T>();
    }

    ~Node()
    {
        delete value;
    }

    void Reset()
    {
        pre = nullptr;
        next = nullptr;
        key = -1;
        if (value != nullptr)
            value->Reset();
    }
};
#endif