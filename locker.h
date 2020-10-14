#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

class sem
{
public:
    sem()
    {
        if (sem_init(&m_sem, 0, 0) != 0)
        {
            throw std::exception();
        }
    }

    ~sem()
    {
        sem_destroy(&m_sem);
    }

    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }

    bool post()
    {
        return sem_post(&m_sem) == 0;
    }
private:
    sem_t m_sem;
};

class locker
{
private:
    pthread_mutex_t m_lock;

public:
    locker()
    {
        pthread_mutex_init(&m_lock,0);
    }
    ~locker()
    {
        pthread_mutex_destroy(&m_lock);
    }
    bool dolock()
    { 
        return (pthread_mutex_lock(&m_lock)==0);
    }
    bool unlock()
    {
        return (pthread_mutex_unlock(&m_lock) == 0);
    }
};
#endif