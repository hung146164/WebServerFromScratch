#include <unordered_map>

struct Node
{
    Node *pre{};
    Node *next{};
    int key = 0, val = 0;
};
class LRUCache
{
public:
    Node *head{};
    Node *tail{};
    std::unordered_map<int, Node *> dp;
    int cnt = 0;
    int capacity = 0;
    LRUCache(int capacity_)
    {
        this->capacity = capacity_;
        dp.reserve(capacity_);
    }

    int get(int key)
    {
        if (dp.find(key) == dp.end())
            return -1;
        Node *curr = dp[key];

        if (curr != head)
        {
            if (curr == tail)
                tail = tail->pre;
            unlock_and_move(curr);
        }
        return curr->val;
    }
    void unlock_and_move(Node *curr)
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
    void put(int key, int value)
    {
        if (dp.find(key) == dp.end())
        {
            if (cnt == capacity)
            {
                int rm_key = tail->key;
                // cout<<"put: "<<key<<' '<<value<<' '<<rm_key<<' '<<head->key<<'\n';
                dp.erase(rm_key);
                dp[key] = tail;
                tail = tail->pre;
            }
            else
            {
                dp[key] = new Node();
                cnt++;
            }
        }
        dp[key]->val = value;
        dp[key]->key = key;

        Node *curr = dp[key];
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
};

/**
 * Your LRUCache object will be instantiated and called as such:
 * LRUCache* obj = new LRUCache(capacity);
 * int param_1 = obj->get(key);
 * obj->put(key,value);
 */