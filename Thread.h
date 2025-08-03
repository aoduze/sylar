//
// Created by admin on 2025/8/2.
//

#ifndef THREAD_H
#define THREAD_H

#include <Thread.h>
#include <iostream>
#include <functional>
#include "singleton.h"
#include <memory>
#include <semaphore>
#include <boost/noncopyable.hpp>
#include <semaphore.h>
#include "mutex.h"

namespace sylar {
    class Thread : private boost::noncopyable {
    public:
        typedef std::shared_ptr<Thread> ptr;
        Thread(std::function<void() > cb, const std::string& name);
        ~Thread();

        pid_t getId() const { return m_id; }
        const std::string& getName() const { return m_name; }

        void join();

        static Thread* GetThis();
        static const std::string& GetName();
        static void SetName(const std::string& name);
    private:
        static void* run(void* arg);
    private:
        pid_t m_id = -1;
        pthread_t m_thread = 0;
        std::function<void() > m_cb;
        std::string m_name;
        //创建成功之后执行对应方法
        Semaphore m_semaphore;
    };
}
#endif //THREAD_H
