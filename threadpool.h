#ifndef PROCESSPOOL_H
#define PROCESSPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>

#include "locker.h"

template <typename T>
class pthreadpool
{
public:
    pthreadpool(int max_pthread = 8, int max_request = 10000);
    ~pthreadpool();
    bool append(T* request);

private:
    int max_pthread;
    int max_request;
    pthread_t *pthread_arr;
    std::list<T*> request_list;
    locker list_lock;
    mem list_mem;
    bool stop;

private:
    static void* work(void*);
    void run();
};

template <typename T>
void* pthreadpool<T>::work(void *arg)
{
    pthreadpool *pool = (pthreadpool*)arg;
    pool -> run();
    return pool;
}

template <typename T>
void pthreadpool<T>::run()
{
    while(!stop)
    {
        list_mem.wait();
        list_lock.dolock();
        if (request_list.empty())
        {
            list_lock.unlock();
            continue;
        }
        T* request = request_list.front();
        request_list.pop_front();
        list_lock.unlock();
        if (!request)
        {
            continue;
        }
        request -> process();
    }
}

template <typename T>
bool pthreadpool<T>:: append(T* request)
{
    list_lock.dolock();
    if (list.size() > max_request)
    {
        list_lock.unlock();
        return false;
    }
    request_list.push_back(request);
    list.unlock();
    list_mem.post();
    return true;
}

template <typename T>
pthreadpool<T>::pthreadpool(int max_pthread, int max_request) :
                max_pthread(max_pthread), max_request(max_request), pthread_arr(nullptr), stop(true)
{
    if (max_pthread <= 0 || max_request <= 0)
    {
        throw std::exception();
    }

    pthread_arr = new pthread_t[max_pthread];

    if (!pthread_arr)
    {
        throw std::exception();
    }

    for (int i = 0; i < max_pthread; ++i)
    {
        if (pthread_create(&pthread_arr[i], NULL, work, this) != 0)
        {
            delete [] pthread_arr;
            throw std::exception();
        }
        if (pthread_detach(pthread_arr[i]) != 0)
        {
            delete [] pthread_arr;
            throw std::exception();
        }
    }
}

template <typename T>
pthreadpool<T>::~pthreadpool()
{
    delete [] pthread_arr;
    stop = true;
}

#endif