/*!
    \file LRU.h
    \brief LRU cache
    \author HungForre
    \date 6/6/2026
    \copyright VDT
*/

#ifndef CPPSERVER_COMMON_LRU_H
#define CPPSERVER_COMMON_LRU_H

#include <unordered_map>

#include "ObjectPool.h"
#include "Node.h"

template <typename T>
class LRUClient
{
private:
    Node<T> *head = nullptr;
    Node<T> *tail = nullptr;
    std::unordered_map<int, Node<T> *> dp;
    int cnt = 0;
    int capacity = 0;
    ObjectPool<Node<T>> pool;

    void detach(Node<T> *node)
    {
        if (node->pre)
            node->pre->next = node->next;
        else
            head = node->next;

        if (node->next)
            node->next->pre = node->pre;
        else
            tail = node->pre;
    }

    void attach_head(Node<T> *node)
    {
        node->next = head;
        node->pre = nullptr;
        if (head)
            head->pre = node;

        head = node;

        if (!tail)
            tail = node;
    }

public:
    LRUClient(int capacity_) : capacity(capacity_)
    {
        for (int i = 0; i < capacity_; i++)
        {
            pool.release(new Node<T>());
        }

        dp.reserve(static_cast<typename std::unordered_map<int, Node<T> *>::size_type>(capacity_));
    }

    T *get(int key)
    {
        auto it = dp.find(key);
        if (it == dp.end())
            return nullptr;

        Node<T> *curr = it->second;
        if (curr != head)
        {
            detach(curr);
            attach_head(curr);
        }

        return curr->value.get();
    }

    void put(int key)
    {
        if (cnt >= capacity)
        {
            int old_key = tail->key;
            Node<T> *old_node = tail;

            detach(old_node);
            dp.erase(old_key);

            old_node->Reset();
            pool.release(old_node);
            cnt--;
        }

        Node<T> *new_node = pool.acquire();
        new_node->key = key;

        attach_head(new_node);
        dp[key] = new_node;

        cnt++;
    }

    void remove(int key)
    {
        auto it = dp.find(key);
        if (it != dp.end())
        {
            Node<T> *node = it->second;
            detach(node);
            dp.erase(it);
            node->Reset();
            pool.release(node);
            cnt--;
        }
    }

    bool full()
    {
        return cnt == capacity;
    }

    int oldestKey()
    {
        return tail->key;
    }
};
#endif