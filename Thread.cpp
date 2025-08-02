//
// Created by admin on 2025/8/2.
//

#include "Thread.h"
#include "log.h"

namespace sylar {
    static thread_local Thread* t_thread = nullptr;
    static thread_local std::string t_thread_name = "UNKNOW";

    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    Thread* Thread::GetThis() {
        return t_thread;
    }

    const std::string& Thread::GetName() {
        return t_thread_name;
    }

    void Thread::SetName(const std::string& name) {
        if(name.empty()) {
            return;
        }
        if(t_thread) {
            t_thread->m_name = name;
        }
        t_thread_name = name;
    }

    Thread::Thread(std::function<void()> cb ,const std::string& name)
        : m_cb(cb)
        , m_name(name) {
        if (name.empty()) {
            m_name = "UNKNOW";
        }
        int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
        if (rt) {
            SYLAR_LOG_ERROR(g_logger) << "pthread_create thread fail, rt=" << rt
                                     << " name=" << name;
            throw std::logic_error("pthread_create error");
        }
        m_semaphore.wait();
    }
}