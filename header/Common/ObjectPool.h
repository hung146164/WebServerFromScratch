#pragma once

#include "CppConfig.h"

template <typename T>
class ObjectPool
{
private:
    std::vector<T *> pool;

public:
    T *acquire()
    {
        if (pool.empty())
        {
            return new T();
        }
        T *obj = pool.back();
        pool.pop_back();
        return obj;
    }

    void release(T *obj)
    {
        if (obj)
        {
            pool.push_back(obj);
        }
    }

    ~ObjectPool()
    {
        for (T *obj : pool)
            delete obj;
    }
};