/*!
    \file LRUCustom.h
    \brief LRUCustom Defination
    \author HungForre
    \date 6/6/2026
    \copyright VDT
*/
#ifndef CPPSERVER_COMMON_LRU_H
#define CPPSERVER_COMMON_LRU_H

#include <unordered_map>
#include <vector>

#include "Node.h"

template <typename T>
class LRUCustom
{
private:
    std::unordered_map<int, int> dp;
    std::vector<Node<T>> nodes;
    std::vector<int> next;

    int head = -1;
    int tail = -1;
    size_t capacity = 0;
    int free_node_idx = -1;

    void Detach(int idx)
    {
        if (nodes[idx].pre != -1)
        {
            nodes[nodes[idx].pre].next = nodes[idx].next;
        }
        else
            head = nodes[idx].next;

        if (nodes[idx].next != -1)
            nodes[nodes[idx].next].pre = nodes[idx].pre;
        else
            tail = nodes[idx].pre;

        nodes[idx].pre = -1;
        nodes[idx].next = -1;
    }

    void AttachHead(int idx)
    {
        nodes[idx].next = head;
        nodes[idx].pre = -1;

        if (head != -1)
            nodes[head].pre = idx;
        head = idx;

        if (tail == -1)
            tail = idx;
    }

public:
    explicit LRUCustom(size_t capacity_) : capacity(capacity_)
    {
        nodes.resize(capacity_);
        next.resize(capacity_, -1);
        dp.reserve(capacity_);
        free_node_idx = 0;

        for (int i = 1; i < capacity_; i++)
        {
            next[i - 1] = i;
        }
    }

    T *Get(int key)
    {
        auto it = dp.find(key);
        if (it == dp.end())
            return nullptr;

        nodes[it->second].last_active_time = time(nullptr);
        if (it->second != head)
        {
            Detach(it->second);
            AttachHead(it->second);
        }
        return &nodes[it->second].value;
    }
    T * GetWithoutMove(int key)
    {
        auto it = dp.find(key);
        if (it == dp.end())
            return nullptr;

        nodes[it->second].last_active_time = time(nullptr);
        return &nodes[it->second].value;
    }
    void Put(int key)
    {
        auto it = dp.find(key);
        if (it != dp.end())
        {
            int idx = it->second;
            nodes[idx].last_active_time = time(nullptr);
            if (idx != head)
            {
                Detach(idx);
                AttachHead(idx);
            }
            return;
        }

        if (free_node_idx == -1)
        {
            int delete_node_idx = tail;
            int old_key = nodes[delete_node_idx].key;

            Detach(delete_node_idx);
            dp.erase(old_key);
            nodes[delete_node_idx].Reset();

            next[delete_node_idx] = free_node_idx;
            free_node_idx = delete_node_idx;
        }

        int curr_idx = free_node_idx;
        free_node_idx = next[free_node_idx];

        nodes[curr_idx].key = key;
        nodes[curr_idx].last_active_time = time(nullptr);

        dp[key] = curr_idx;
        AttachHead(curr_idx);
    }
    bool Remove(int key)
    {
        auto it = dp.find(key);
        if (it == dp.end())
            return false;

        int idx = it->second;
        Detach(idx);
        dp.erase(it);
        nodes[idx].Reset();

        next[idx] = free_node_idx;
        free_node_idx = idx;

        return true;
    }
    void MakeRecent(int fd)
    {
        auto it = dp.find(fd);
        if (it != dp.end())
        {
            int idx = it->second;
            nodes[idx].last_active_time = time(nullptr);
            if (idx != head)
            {
                Detach(idx);
                AttachHead(idx);
            }
        }
    }

    time_t GetLastActiveTime(int key)
    {
        auto it = dp.find(key);
        if (it == dp.end())
            return 0;
        return nodes[it->second].last_active_time;
    }

    bool Full() {
        return free_node_idx == -1; }
    int OldestKey() {
        if (tail == -1) return -1;
        return nodes[tail].key;
    }
};
#endif
