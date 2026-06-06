/*!
    \file ObjectPool.h
    \brief ObjectPool
    \author HungForre
    \date 6/6/2026
    \copyright VDT
*/

#ifndef CPPSERVER_COMMON_OBJECTPOOL_H
#define CPPSERVER_COMMON_OBJECTPOOL_H

#include <vector>

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

#endif