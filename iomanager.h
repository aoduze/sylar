//
// Created by admin on 2025/8/11.
//

#ifndef IOMANAGER_H
#define IOMANAGER_H

#include "Schedule.h"
#include "timer.h"

namespace sylar {

    // 基于Epoll的IO协程调度器
    class IOManager : public Scheduler, public TimerManager {
    public:
        typedef std::shared_ptr<IOManager> ptr;
        typedef RWMutex RWMutexType;

        enum Event {
            // 无事件
            NONE = 0x0,
            // 读事件（EPOLLIN）
            READ = 0x1,
            // 写事件(EPOLLOUT)
            WRITE = 0x4,
        };
    private:
        struct FdContext {
            typedef Mutex MutexType;
            struct EventContext {
                // 事件调度器
                Scheduler* scheduler = nullptr;
                // 事件协程
                Fiber::ptr fiber;
                std::function<void()> cb;
            };

            // 获取上下文类
            EventContext& getContext(Event event);

            // 重置事件上下文
            void resetContext(EventContext& ctx);

            // 触发事件
            void triggerEvent(Event event);

            // 读事件上下文
            EventContext read;
            // 写事件上下文
            EventContext write;
            // 事件关联的句柄
            int fd = 0;
            // 当前的事件
            Event events = NONE;
            // 事件锁
            MutexType mutex;
        };
    public:
        // threads 线程数量
        // use_caller 是否将调用线程包含进去
        // name 调度器名称
        IOManager(size_t threads = 1, bool use_caller = true, const std::string& name = "");

        // 析构函数
        ~IOManager();

        // 添加事件
        // 成功返回0,失败返回-1
        int addEvent(int fd, Event event, std::function<void()> cb = nullptr);

        // 删除事件
        bool delEvent(int fd, Event event);

        // 取消事件
        bool cancelEvent(int fd, Event event);

        // 取消所有事件
        bool cancelAll(int fd);

        // 返回当前IOMANAGER
        static IOManager* GetThis();
    protected:
        void tickle() override;
        bool stopping() override;
        void idle() override;
        void onTimerInsertedAtFront() override;

        // 重置上下文容量大小
        void contextResize(size_t size);

        // 是否可以停止
        bool stopping(uint64_t& timeout);
    private:
        /// epoll 文件句柄
        int m_epfd = 0;
        /// pipe 文件句柄
        int m_tickleFds[2];
        /// 当前等待执行的事件数量
        std::atomic<size_t> m_pendingEventCount = {0};
        /// IOManager的Mutex
        RWMutexType m_mutex;
        /// socket事件上下文的容器
        std::vector<FdContext*> m_fdContexts;
    };
}
#endif //IOMANAGER_H
