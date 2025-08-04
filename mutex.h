//
// Created by admin on 2025/8/2.
//

#ifndef MUTEX_H
#define MUTEX_H

#include <thread>
#include <functional>
#include <memory>
#include <pthread.h>
#include <stdint.h>
#include <atomic>
#include <list>
#include <semaphore.h>
#include <boost/noncopyable.hpp>


namespace sylar {
    //信号量
    class Semaphore : private boost::noncopyable {
    public:
        Semaphore(uint32_t count = 0);
        ~Semaphore();

        void wait();
        void notify();
    private:
        sem_t m_semaphore;
    };

    //局部锁模板
    template<class T>
    struct ScopedLockImpl {
    public:
        ScopedLockImpl(T& mutex)
            :m_mutex(mutex) {
            m_mutex.lock();
            m_locked = true;
        }

        ~ScopedLockImpl() {
            unlock();
        }

        void lock() {
            if (!m_locked) {
                m_mutex.lock();
                m_locked = true;
            }
        }

        void unlock() {
            if (m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }
    private:
        T& m_mutex;
        bool m_locked;
    };

    //读锁
    template<class T>
    struct ReadScopedLockImpl {
    public:
        ReadScopedLockImpl(T& mutex)
            :m_mutex(mutex) {
            m_mutex.lock();
            m_locked = true;
        }

        ~ReadScopedLockImpl() {
            unlock();
        }

        void lock() {
            if (m_locked) {
                m_mutex.lock();
                m_locked = true;
            }
        }

        void unlock() {
            if (m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }
    private:
        T& m_mutex;
        bool m_locked;
    };

    //写锁
    template<typename T>
    class WriteScopedLockImpl {
    public:
        WriteScopedLockImpl(T& mutex)
            :m_mutex(mutex) {
            m_mutex.wrlock();
            m_locked = true;
        }

        ~WriteScopedLockImpl() {
            unlock();
        }

        void lock() {
            if (!m_locked) {
                m_mutex.lock();
                m_locked = true;
            }
        }

        void unlock() {
            if (m_locked) {
                m_mutex.unlock();
                m_locked = false;
            }
        }
    private:
        T& m_mutex;
        bool m_locked;
    };

    //互斥量
    class Mutex : private boost::noncopyable {
    public:
        Mutex() {
            pthread_mutex_init(&m_mutex, nullptr);
        }

        ~ Mutex() {
            pthread_mutex_destroy(&m_mutex);
        }

        void lock() {
            pthread_mutex_lock(&m_mutex);
        }

        void unlock() {
            pthread_mutex_unlock(&m_mutex);
        }
    private:
        pthread_mutex_t m_mutex;
    };

    class NullMutex : private boost::noncopyable {
    public:
        NullMutex() {}

        ~NullMutex() {}

        void lock() {}

        void unlock() {}
    };

    //读写互斥量
    class RWMutex : private boost::noncopyable {
    public:
        RWMutex() {
            pthread_rwlock_init(&m_lock, nullptr);
        }

        ~RWMutex() {
            pthread_rwlock_destroy(&m_lock);
        }

        void rdlock() {
            pthread_rwlock_rdlock(&m_lock);
        }

        void wrlock() {
            pthread_rwlock_wrlock(&m_lock);
        }

        void unlock() {
            pthread_rwlock_unlock(&m_lock);
        }
    private:
        pthread_rwlock_t m_lock;
    };

    //空读写锁,通常用于调试
    class NullRWMutex : private boost::noncopyable {
    public:
        typedef ReadScopedLockImpl<NullMutex> ReadLock;
        typedef WriteScopedLockImpl<NullMutex> WriteLock;
        NullRWMutex() {}
        ~NullRWMutex() {}
        void rdlock() {}
        void wrlock() {}
        void unlock() {}
    };

    //自旋锁
    class Spinlock : private boost::noncopyable {
    public:
        //局部锁
        typedef ScopedLockImpl<Spinlock> Lock;

        Spinlock() {
            pthread_spin_init(&m_mutex, 0);
        }

        ~Spinlock() {
            pthread_spin_destroy(&m_mutex);
        }

        void lock() {
            pthread_spin_lock(&m_mutex);
        }

        void unlock() {
            pthread_spin_unlock(&m_mutex);
        }
    private:
        pthread_spinlock_t m_mutex;
    };

    class CASLock : private boost::noncopyable {
    public:
        typedef ScopedLockImpl<CASLock> Lock;

        CASLock() {
            m_mutex.clear();
        }

        ~CASLock() {
        }

        void lock() {
            while (std::atomic_flag_test_and_set_explicit(&m_mutex,
                std::memory_order_acquire));
        }

        void unlock() {
            std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
        }
    private:
        std::atomic_flag m_mutex = ATOMIC_FLAG_INIT;
    };
}

#endif //MUTEX_H
