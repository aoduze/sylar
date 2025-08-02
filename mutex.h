//
// Created by admin on 2025/8/2.
//

#ifndef MUTEX_H
#define MUTEX_H

#include <atomic>
#include <list>
#include <semaphore.h>
#include <boost/noncopyable.hpp>


namespace sylar {
    class Semaphore : private boost::noncopyable {
    public:
        /**
         * @brief 构造函数
         * @param[in] count 信号量值的大小
         */
        Semaphore(uint32_t count = 0);

        /**
         * @brief 析构函数
         */
        ~Semaphore();

        /**
         * @brief 获取信号量
         */
        void wait();

        /**
         * @brief 释放信号量
         */
        void notify();
    private:
        sem_t m_semaphore;
    };
    class mutex {

    };
}

#endif //MUTEX_H
