#pragma once

#include <pthread.h>

class MutexGuard
{
public:
    MutexGuard(pthread_mutex_t &_mtx, bool _lock = true)
        : mtx(_mtx), isLock(_lock)
    {
        if (isLock)
        {
            pthread_mutex_lock(&mtx);
        }
    }
    virtual ~MutexGuard()
    {
        if (isLock)
        {
            pthread_mutex_unlock(&mtx);
        }
    }
    void lock()
    {
        if (! isLock)
        {
            pthread_mutex_unlock(&mtx);
            isLock = true;
        }
    }
    void unlock()
    {
        if (isLock)
        {
            pthread_mutex_unlock(&mtx);
            isLock = false;
        }
    }
protected:
    pthread_mutex_t &mtx;
    bool isLock;
};
