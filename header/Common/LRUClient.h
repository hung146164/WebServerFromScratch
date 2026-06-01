#include "ObjectPool.h"
#include "CppConfig.h"
#include "Http/HttpRequest.h"

struct Node
{
    Node *pre{};
    Node *next{};
    int key = -1;
    HttpRequest *value{};

    Node()
    {
        value = new HttpRequest();
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

class LRUClient
{
private:
    Node *head = nullptr;
    Node *tail = nullptr;
    std::unordered_map<int, Node *> dp;
    int cnt = 0;
    int capacity = 0;
    ObjectPool<Node> pool;

    LRUClient()
    {
        for (int i = 0; i < 1000; i++)
        {
            Node *node = new Node();
            pool.release(node);
        }
    }

    void detach(Node *node)
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

    void attach_head(Node *node)
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
        dp.reserve(capacity_);
    }

    HttpRequest *get(int key)
    {
        auto it = dp.find(key);
        if (it == dp.end())
            return nullptr;

        Node *curr = it->second;
        if (curr != head)
        {
            detach(curr);
            attach_head(curr);
        }
        return curr->value;
    }

    void put(int key)
    {
        if (cnt >= capacity)
        {
            int old_key = tail->key;
            Node *old_node = tail;

            // erase in LRU
            detach(old_node);
            dp.erase(old_key);

            // add to pool
            old_node->Reset();
            pool.release(old_node);
            cnt--;
        }

        // Thêm node mới
        Node *new_node = pool.acquire();
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
            Node *node = it->second;
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