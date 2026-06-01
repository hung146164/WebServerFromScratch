#include "ObjectPool.h"

#include "CppConfig.h"

template <typename T>
struct Node
{
    Node *pre{};
    Node *next{};
    int key = -1;
    T *value{};
    void ResetNode()
    {
        pre = nullptr;
        next = nullptr;
        key = -1;
        value = nullptr;
    }
};

template <typename T>
class LRUCache
{
private:
    Node<T> *head{};
    Node<T> *tail{};
    std::unordered_map<int, Node<T> *> dp;
    int cnt = 0;
    int capacity = 0;

    ObjectPool<Node<T>> pool;
    void remove(Node<T> *node)
    {
        if (node->pre != nullptr)
        {
            node->pre->next = node->next;
        }
        if (node->next != nullptr)
        {
            node->next->pre = node->pre;
        }
        node->ResetNode();
        pool.release(node);
        cnt--;
    }
    void unlock_and_move(Node<T> *curr)
    {
        // unlock
        if (curr->next != nullptr)
        {
            curr->next->pre = curr->pre;
        }
        if (curr->pre != nullptr)
        {
            curr->pre->next = curr->next;
        }

        // move forward
        curr->next = head;
        if (head != nullptr)
        {
            head->pre = curr;
        }
        curr->pre = nullptr;
        head = curr;
    }

public:
    LRUCache(int capacity_)
    {
        this->capacity = capacity_;
        dp.reserve(capacity_);
    }

    int size() const
    {
        return cnt;
    }

    bool full() const
    {
        return cnt >= capacity;
    }

    int oldestKey() const
    {
        return tail ? tail->key : -1;
    }

    T *get(int key)
    {
        if (dp.find(key) == dp.end())
            return nullptr;
        Node<T> *curr = dp[key];

        if (curr != head)
        {
            if (curr == tail)
                tail = tail->pre;
            unlock_and_move(curr);
        }
        return curr->value;
    }

    void put(int key, T *value)
    {
        if (dp.find(key) == dp.end())
        {
            if (cnt == capacity)
            {
                int rm_key = tail->key;
                dp.erase(rm_key);
                dp[key] = tail;
                tail = tail->pre;
            }
            else
            {
                dp[key] = pool.acquire();
                cnt++;
            }
        }
        dp[key]->value = value;
        dp[key]->key = key;

        Node<T> *curr = dp[key];
        if (curr != head)
        {
            if (curr == tail)
                tail = tail->pre;
            unlock_and_move(curr);
        }
        // if first insert
        if (tail == nullptr)
            tail = curr;
    }

    void remove(int fd)
    {
        if (dp.find(fd) == dp.end())
            return;
        remove(dp[fd]);
    }
};

/**
 * Your LRUCache object will be instantiated and called as such:
 * LRUCache* obj = new LRUCache(capacity);
 * int param_1 = obj->get(key);
 * obj->put(key,value);
 */