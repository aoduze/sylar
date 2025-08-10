//
// Created by admin on 2025/8/8.
//

#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <memory>
#include <vector>
#include <list>
#include <iostream>
#include "fiber.h"
#include "Thread.h"
#include <google/protobuf/message.h>

namespace sylar {

    class Scheduler {
    public:
        // 协程调度器
        typedef std::shared_ptr<Scheduler> ptr;
        typedef Mutex MutexType;

        // use_caller 为是否调用当前线程, threads 为线程数量
        Scheduler(size_t threads = 1, bool use_caller = true, const std::string& name = "");

        virtual ~Scheduler();

        const std::string& getName() const {return m_name;}

        // 返回当前协程调度器
        static Scheduler* GetThis();

        // 返回当前协程调度器的调度协程
        static Fiber* GetMainFiber();

        // 启动协程调度器
        void start();

        // 停止协程调度器
        void stop();

        /*
         * 调度协程
         * fc : 协程或函数
         * thread : 协程执行的线程id, -1 为任意线程
        */
        template<class FiberOrCb>
        void schedule(FiberOrCb fc, int thread = -1) {
            bool need_tickle = false;
            {
                MutexType::Lock lock(m_mutex);
                need_tickle = scheduleNoLock(fc, thread);
            }

            if(need_tickle) {
                tickle();
            }
        }

        /**
         * 批量调度协程
         * begin是数组的开始
         * end 协程数组的结束
         */
        template<class InputIterator>
        void schedule(InputIterator begin, InputIterator end) {
            bool need_tickle = false;
            {
                MutexType::Lock lock(m_mutex);
                while(begin != end) {
                    need_tickle = scheduleNoLock(*begin, -1) || need_tickle;
                    ++begin;
                }
            }
            if(need_tickle) {
                tickle();
            }
        }

        void switchTo(int thread = -1);
        std::ostream& dump(std::ostream& os);

    protected:
        // 通知协程调度器有任务了,线程空闲时会阻塞
        virtual void tickle();

        void run();

        virtual bool stopping();

        virtual void idle();

        void setThis();

        bool hasIdleThreads() { return m_idleThreadCount > 0; }

    private:
        // 启动协程调度(无锁)
        template <class FiberOrCb>
        bool scheduleNoLock(FiberOrCb fc, int thread) {
            bool need_tickle = m_fibers.empty();
            FiberAndThread ft(fc, thread);
            if(ft.fiber || ft.cb) {
                m_fibers.push_back(ft);
            }
            return need_tickle;
        }

    private:
        struct FiberAndThread {
            Fiber::ptr fiber;
            std::function<void()> cb;
            int thread;

            // 构造
            FiberAndThread(Fiber::ptr f, int thr)
                :fiber(f), thread(thr) {
            }
            // 构造, 唯一的不同是指针函数
            FiberAndThread(Fiber::ptr* f, int thr)
                :thread(thr) {
                fiber.swap(*f);
            }

            // 构造函数, 函数版本
            FiberAndThread(std::function<void()> f, int thr)
                :cb(f), thread(thr) {
            }

            // 构造函数, 函数指针版本
            FiberAndThread(std::function<void()>* f, int thr)
                :thread(thr) {
                cb.swap(*f);
            }

            FiberAndThread()
                : thread(-1) {

            }

            void reset() {
                fiber.reset();
                cb = nullptr;
            }
        };
    private:
        MutexType m_mutex;
        // 线程池
        std::vector<Thread::ptr> m_threads;
        // 待执行任务队列
        std::list<FiberAndThread> m_fibers;
        // use_caller 为 true 时调度协程
        Fiber::ptr m_rootFiber;
        // 名字
        std::string m_name;
    protected:
        // 协程下的线程ID数组
        std::vector<int> m_threadIds;
        // 线程数量
        size_t m_threadCount = 0;
        // 工作线程数量
        std::atomic<size_t> m_activeThreadCount = 0;
        // 空闲线程数量
        std::atomic<size_t> m_idleThreadCount = 0;
        // 线程池是否正在停止
        bool m_stopping = true;
        // 是否自动停止
        bool m_autoStop = false;
        // 主线程id
        int m_rootThread = 0;
    };

    class SchedulerSwitcher {
    public:
        SchedulerSwitcher(Scheduler* target = nullptr);
        ~SchedulerSwitcher();
    private:
        Scheduler* m_caller;
    };
}

#endif //SCHEDULE_H
