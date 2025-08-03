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
        //等待操作执行完毕
        m_semaphore.wait();
    }

    Thread::~Thread() {
        if (m_thread) {
            pthread_detach(m_thread);
        }
    }

    void Thread::join() {
        //使用pthread_join等待线程结束
        int rt = pthread_join(m_thread, nullptr);
        if (rt) {
            SYLAR_LOG_ERROR(g_logger) << "pthread_join thread fail" << rt
                << "name = " << m_name;
            throw std::logic_error("pthread_join error");
        }
    }

    void* Thread::run(void * arg) {

        //首先要将void*类型的指针转换为Thread类型的指针
        Thread* thread = (Thread*)arg;

        t_thread = thread;
        t_thread_name = thread->m_name;
        thread->m_id = sylar::GetThreadId();
        //设置线程名称
        pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

        //thread->m_cb中用户提供的回调函数
        //这里我们采用swap的好处是性能优势和异常安全以及资源管理
        std::function<void()> cb;
        cb.swap(thread->m_cb);
        thread->m_semaphore.notify();
        cb();
        return 0;
    }
}