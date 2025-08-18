//
// Created by admin on 2025/8/11.
//

#include "iomanager.h"
#include "macro.h"
#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>

namespace sylar {
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    enum EpollCtlOp {};

    static std::ostream& operator<< (std::ostream& os, const EpollCtlOp& op) {
        switch((int)op) {
#define XX(ctl) \
case ctl: \
return os << #ctl;
            XX(EPOLL_CTL_ADD);
            XX(EPOLL_CTL_MOD);
            XX(EPOLL_CTL_DEL);
            default:
            return os << (int)op;
        }
#undef XX
    }

    static std::ostream& operator<< (std::ostream& os, EPOLL_EVENTS events) {
        if(!events) {
            return os << "0";
        }
        bool first = true;
#define XX(E) \
if(events & E) { \
if(!first) { \
os << "|"; \
} \
os << #E; \
first = false; \
}
        XX(EPOLLIN);
        XX(EPOLLPRI);
        XX(EPOLLOUT);
        XX(EPOLLRDNORM);
        XX(EPOLLRDBAND);
        XX(EPOLLWRNORM);
        XX(EPOLLWRBAND);
        XX(EPOLLMSG);
        XX(EPOLLERR);
        XX(EPOLLHUP);
        XX(EPOLLRDHUP);
        XX(EPOLLONESHOT);
        XX(EPOLLET);
#undef XX
        return os;
    }

    // 判断上下文类型
    IOManager::FdContext::EventContext& IOManager::FdContext::getContext(
        IOManager::Event event) {
        switch (event) {
            case IOManager::READ:
                return read;
            case IOManager::WRITE:
                return write;
            default:
                SYLAR_ASSERT2(false, "getContext");
        }
        throw std::invalid_argument("getContext invalid event");
    }

    // 重置一下上下文结构
    void IOManager::FdContext::resetContext(EventContext& ctx) {
        ctx.scheduler = nullptr;
        ctx.fiber.reset();
        ctx.cb = nullptr;
    }

    // 触发指定的IO事件, 并将对应的协程或回调函数交给调度器执行
    void IOManager::FdContext::triggerEvent(IOManager::Event event) {
        SYALR_ASSERT(events & event);

        // 从events中移除指定的event
        // 当某个事件被触发后, 需要从注册列表中移除它
        // 这样可以避免重复触发同一个事件
        events = (Event)(events & ~event);
        EventContext& ctx = getContext(event);
        if(ctx.cb) {
            ctx.scheduler->schedule(&ctx.cb);
        } else {
            ctx.scheduler->schedule(ctx.fiber);
        }
        ctx.scheduler = nullptr;
    }


    // 构造函数
    IOManager::IOManager(size_t threads, bool use_caller, const std::string& name)
    :Scheduler(threads, use_caller, name) {
        // 我们期望epoll的句柄上限是5000
        m_epfd = epoll_create(5000);
        SYLAR_ASSERT(m_epfd > 0);

        // 返回读端和写端的两个文件描述符
        int rt = pipe(m_tickleFds);
        SYLAR_ASSERT(!rt);

        // 配置epoll事件
        epoll_event event;
        memset(&event, 0, sizeof(epoll_event));
        // 监听可读事件，设置边缘触发模式
        event.events = EPOLLIN | EPOLLET;
        // 关联读端文件描述符
        event.data.fd = m_tickleFds[0];

        // 将读端设置为非阻塞模式
        rt = fcntl(m_tickleFds[0], F_SETFL, O_NONBLOCK);
        SYLAR_ASSERT(!rt);

        // 将管道注册到epoll， 文件描述符添加到epoll中
        rt = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_tickleFds[0], &event);
        SYLAR_ASSERT(!rt);

        // 预分配32个FdContext对象
        contextResize(32);

        start();
    }

    IOManager::~IOManager() {
        stop();
        close(m_epfd);
        close(m_tickleFds[0]);
        close(m_tickleFds[1]);

        // 逐个删除事件
        for(size_t i = 0; i < m_fdContexts.size(); ++i) {
            if(m_fdContexts[i]) {
                delete m_fdContexts[i];
            }
        }
    }

    void IOManager::contextResize(size_t size) {
        m_fdContexts.resize(size);
        for(size_t i = 0; i < m_fdContexts.size(); ++i) {
            if(!m_fdContexts[i]) {
                m_fdContexts[i] = new FdContext;
            }
        }
    }

    int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
        FdContext* fd_ctx = nullptr;
        RWMutexType::ReadLock lock(m_mutex);
        // 我们必须确保容器大小和fd大小的状态,防止月结
        if((int)m_fdContexts.size() > fd) {
            fd_ctx = m_fdContexts[fd];
            lock.unlock();
        } else {
            lock.unlock();
            RWMutexType::WriteLock lock2(m_mutex);
            contextResize(fd * 1.5);
            fd_ctx = m_fdContexts[fd];
        }

        // 加锁并输出基本信息
        FdContext::MutexType::Lock lock2(fd_ctx->mutex);
        if(SYLAR_UNLIKELY(fd_ctx -> events & event)) {
            SYLAR_LOG_ERROR(g_logger) << "addEvent assert fd=" << fd
                        << " event=" << (EPOLL_EVENTS)event
                        << " fd_ctx.event=" << (EPOLL_EVENTS)fd_ctx->events;
            SYLAR_ASSERT(!(fd_ctx->events & event));
        }

        // Event是我们的事件等级

        // 确定epoll的操作类型为添加还是修改
        int op = fd_ctx -> events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
        // 创建epoll事件结构
        epoll_event epevent;
        // 设置事件类型 (边缘触发模式, 当前已注册事件, 新要添加的事件)
        epevent.events = EPOLLET | fd_ctx -> events | event;
        // 设置关联数据
        epevent.data.ptr = fd_ctx;

        // 将事件添加到epoll
        int rt = epoll_ctl(m_epfd, op, fd, &epevent);
        if(rt) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
                << rt << " (" << errno << ") (" << strerror(errno) << ") fd_ctx->events="
                << (EPOLL_EVENTS)fd_ctx->events;
            return -1;
        }

        // 增加当前等待执行的事件数量
        ++m_pendingEventCount;
        // 更新文件描述符上下文中已注册的事件标志
        fd_ctx -> events = (Event)(fd_ctx -> events | event);
        // 获取上下文, event_ctx保存当前状态
        FdContext::EventContext& event_ctx = fd_ctx -> getContext(event);
        SYLAR_ASSERT(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.cb);

        // 设置调度器, 协程, 回调函数
        event_ctx.scheduler = Scheduler::GetThis();
        if(cb) {
            event_ctx.cb.swap(cb);
        } else {
            event_ctx.fiber = Fiber::GetThis();
            SYLAR_ASSERT2(event_ctx.fiber->getState() == Fiber::EXEC
              ,"state=" << event_ctx.fiber->getState());
        }
        return 0;
    }

    bool IOManager::delEvent(int fd, Event event) {
        RWMutexType::ReadLock lock(m_mutex);
        if((int)m_fdContexts.size() <= fd) {
            return false;
        }
        FdContext* fd_ctx = m_fdContexts[fd];
        lock.unlock();

        FdContext::MutexType::Lock lock2(fd_ctx->mutex);
        if(!(fd_ctx->events & event)) {
            return false;
        }

        // 确定类型
        Event new_events = (Event)(fd_ctx -> events & ~event);
        int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
        epoll_event epevent;
        epevnt.events = EPOLLET | new_events;
        epevent.data.ptr = fd_ctx;

        int rt = epoll_ctl(m_epfd, op, fd, &epevent);
        if (rt) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
                << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }

        --m_pendingEventCount;
        fd_ctx -> events = new_events;
        FdContext::EventContext& event_ctx = fd_ctx -> getContext(event);
        // 重置事件上下文
        fd_ctx -> resetContext(event_ctx);
        return true;
    }

    bool IOManager::cancelEvent(int fd, Event event) {
        RWMutexType::ReadLock lock(m_mutex);
        if((int)m_fdContexts.size() <= fd) {
            return false;
        }
        FdContext* fd_ctx = m_fdContexts[fd];
        lock.unlock();

        FdContext::MutexType::Lock lock2(fd_ctx->mutex);
        // 检测是否已经注册
        if(!(fd_ctx -> events & event)) {
            return false;
        }

        // 更新状态, 移除指定事件
        Event new_events = (Event)(fd_ctx -> events & ~event);
        int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
        epoll_event epevent;
        epevent.events = EPOLLET | new_events;
        epevent.data.ptr = fd_ctx;

        int rt = epoll_ctl(m_epfd, op, fd, &epevent);
        if (rt) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
                << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }

        // 重置事件上下文
        fd_ctx -> resetContext(fd_ctx -> getContext(event));
        --m_pendingEventCount;
        return true;
    }

    bool IOManager::cancelAll(int fd) {
        RWMutexType::ReadLock lock(m_mutex);
        if((int)m_fdContexts.size() <= fd) {
            return false;
        }
        FdContext* fd_ctx = m_fdContexts[fd];
        lock.unlock();

        FdContext::MutexType::Lock lock2(fd_ctx->mutex);
        if(!fd_ctx->events) {
            return false;
        }

        // 选择删除
        int op = EPOLL_CTL_DEL;
        epoll_event epevent;
        // 删除操作,我们要把事件类型设为0
        epevent.events = 0;
        epevent.data.ptr = fd_ctx;

        // 执行epoll调用
        int rt = epoll_ctl(m_epfd, op, fd, &epevent);
        if (rt) {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                << (EpollCtlOp)op << ", " << fd << ", " << (EPOLL_EVENTS)epevent.events << "):"
                << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }

        /**
         * 我们用位运算来区分读事件和写事件
         * 如果这里我们一股脑reset的话
         * 如果类型不匹配,会发生严重错误
         */
        if(fd_ctx->events & READ) {
            fd_ctx->resetContext(fd_ctx->read);
            --m_pendingEventCount;
        }
        if(fd_ctx->events & WRITE) {
            fd_ctx->resetContext(fd_ctx->write);
            --m_pendingEventCount;
        }
        SYLAR_ASSERT(fd_ctx -> events == 0);
        return true;
    }

    IOManager* IOManager::GetThis() {
        // GetThis返回的是Scheduler*,所以我们这里dynamic_cast了
        // 但请注意, 我们需要注意安全使用
        return dynamic_cast<IOManager*>(Scheduler::GetThis());
    }

    // 唤醒空闲线程
    void IOManager::tickle() {
        if (!hasIdleThreads()) {
            return ;
        }
        int rt = write(m_tickleFds[1],"T", 1);
        SYLAR_ASSERT(rt == 1);
    }

    bool IOManager::stopping(uint64_t& timeout) {
        // 获取下一个定时器的超时时间
        timeout = getNextTimer();
        return timeout == ~0ull && m_pendingEventCount == 0 && stopping();
    }

    bool IOManager::stopping() {
        uint64_t timeout = 0;
        return stopping(timeout);
    }

    // 负责监听和处理所有的IO事件,定时器事件以及线程间通信
    void IOManager::idle() {
        SYLAR_LOG_DEBUG(g_logger) << "idle";

        // 定义最大事件数量为256
        const uint64_t MAX_EVNETS = 256;

        // 分配epoll事件数组，用于接收epoll_wait返回的事件
        epoll_event* events = new epoll_event[MAX_EVNETS]();

        // 使用智能指针管理事件数组内存，自定义删除器确保正确释放数组
        std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* ptr){
            delete[] ptr;
        });

        // 主事件循环
        while(true) {
            uint64_t next_timeout = 0;
            // 检查IOManager是否应该停止，同时获取下一个定时器的超时时间
            if(SYLAR_UNLIKELY(stopping(next_timeout))) {
                SYLAR_LOG_INFO(g_logger) << "name=" << getName()
                                         << " idle stopping exit";
                break; // 满足停止条件，退出循环
            }

            int rt = 0;
            // epoll_wait调用循环，处理信号中断
            do {
                // 设置最大超时时间为3秒，避免无限等待
                static const int MAX_TIMEOUT = 3000;

                // 计算实际的超时时间
                if(next_timeout != ~0ull) {
                    // 如果有定时器，取定时器时间和最大超时时间的较小值
                    next_timeout = (int)next_timeout > MAX_TIMEOUT
                                    ? MAX_TIMEOUT : next_timeout;
                } else {
                    // 如果没有定时器，使用最大超时时间
                    next_timeout = MAX_TIMEOUT;
                }

                // 等待IO事件发生，最多等待next_timeout毫秒
                rt = epoll_wait(m_epfd, events, MAX_EVNETS, (int)next_timeout);
                // 如果被信号中断，继续等待；否则跳出循环
                if(rt < 0 && errno == EINTR) {
                    // 被信号中断，继续循环
                } else {
                    break; // 正常返回或其他错误，跳出循环
                }
            } while(true);

            // 处理到期的定时器
            std::vector<std::function<void()> > cbs;
            listExpiredCb(cbs); // 获取所有到期的定时器回调函数
            if(!cbs.empty()) {
                // 如果有到期的定时器，将回调函数提交给调度器执行
                //SYLAR_LOG_DEBUG(g_logger) << "on timer cbs.size=" << cbs.size();
                schedule(cbs.begin(), cbs.end());
                cbs.clear(); // 清空回调函数列表
            }

            // 如果事件数量达到最大值，可能需要增加事件数组大小（调试用）
            //if(SYLAR_UNLIKELY(rt == MAX_EVNETS)) {
            //    SYLAR_LOG_INFO(g_logger) << "epoll wait events=" << rt;
            //}

            // 处理所有返回的IO事件
            for(int i = 0; i < rt; ++i) {
                epoll_event& event = events[i];

                // 检查是否是线程唤醒事件（tickle管道的读端）
                if(event.data.fd == m_tickleFds[0]) {
                    uint8_t dummy[256];
                    // 清空管道中的所有数据，消费唤醒信号
                    while(read(m_tickleFds[0], dummy, sizeof(dummy)) > 0);
                    continue; // 处理下一个事件
                }

                // 获取文件描述符上下文（之前通过epevent.data.ptr设置）
                FdContext* fd_ctx = (FdContext*)event.data.ptr;
                // 对文件描述符上下文加锁，确保线程安全
                FdContext::MutexType::Lock lock(fd_ctx->mutex);

                // 处理错误和连接断开事件
                if(event.events & (EPOLLERR | EPOLLHUP)) {
                    // 将错误事件转换为读写事件，让上层应用处理
                    event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
                }

                // 将epoll事件转换为IOManager的事件类型
                int real_events = NONE;
                if(event.events & EPOLLIN) {
                    real_events |= READ; // epoll可读事件转换为READ事件
                }
                if(event.events & EPOLLOUT) {
                    real_events |= WRITE; // epoll可写事件转换为WRITE事件
                }

                // 检查触发的事件是否是我们关心的事件
                if((fd_ctx->events & real_events) == NONE) {
                    continue; // 不是我们关心的事件，跳过
                }

                // 计算处理当前事件后剩余的事件
                int left_events = (fd_ctx->events & ~real_events);
                // 根据剩余事件决定epoll操作：有剩余事件则修改，无剩余事件则删除
                int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
                // 设置新的epoll事件（边缘触发 + 剩余事件）
                event.events = EPOLLET | left_events;

                // 更新epoll的监听状态
                int rt2 = epoll_ctl(m_epfd, op, fd_ctx->fd, &event);
                if(rt2) {
                    // epoll_ctl调用失败，记录错误日志
                    SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << m_epfd << ", "
                        << (EpollCtlOp)op << ", " << fd_ctx->fd << ", " << (EPOLL_EVENTS)event.events << "):"
                        << rt2 << " (" << errno << ") (" << strerror(errno) << ")";
                    continue; // 处理下一个事件
                }

                // 调试日志（已注释）
                //SYLAR_LOG_INFO(g_logger) << " fd=" << fd_ctx->fd << " events=" << fd_ctx->events
                //                         << " real_events=" << real_events;

                // 触发相应的事件处理
                if(real_events & READ) {
                    fd_ctx->triggerEvent(read); // 触发读事件，唤醒等待的协程或执行回调
                    --m_pendingEventCount; // 减少待处理事件计数
                }
                if(real_events & WRITE) {
                    fd_ctx->triggerEvent(WRITE); // 触发写事件，唤醒等待的协程或执行回调
                    --m_pendingEventCount; // 减少待处理事件计数
                }
            }

            // 协程切换：从idle协程切换回调度协程
            Fiber::ptr cur = Fiber::GetThis(); // 获取当前协程（idle协程）
            auto raw_ptr = cur.get(); // 获取原始指针
            cur.reset(); // 释放智能指针，减少引用计数

            raw_ptr->swapOut(); // 切换回调度协程，让调度器继续调度其他任务
        }
    }

    void IOManager::onTimerInsertedAtFront() {
        tickle();
    }
}
