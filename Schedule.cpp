// 协程调度器实现文件
// 实现了基于协程的任务调度器，支持多线程协程调度
// 提供了协程任务的添加、执行、状态管理等功能

#include "Schedule.h"
#include "log.h"
#include "macro.h"
#include "hook.h"

namespace sylar {
    // 系统日志器，用于调度器相关的日志输出
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    // 线程局部变量：当前线程的调度器指针
    static thread_local Scheduler* t_scheduler = nullptr;
    // 线程局部变量：当前线程的调度协程指针
    static thread_local Fiber* t_scheduler_fiber = nullptr;

    // 协程调度器构造函数
    // threads: 线程数量，必须大于0
    // use_caller: 是否使用调用者线程参与调度
    // name: 调度器名称
    // 根据use_caller参数决定调度器的工作模式：
    // - use_caller=true: 调用者线程参与调度，创建N-1个工作线程
    // - use_caller=false: 调用者线程不参与调度，创建N个工作线程
    Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name)
        : m_name(name) {
        SYLAR_ASSERT(threads > 0);

        if (use_caller) {
            // 初始化当前线程的主协程
            sylar::Fiber::GetThis();
            // 调用者线程参与调度，工作线程数减1
            --threads;

            // 确保当前线程没有绑定其他调度器
            SYLAR_ASSERT(GetThis() == nullptr);
            // 设置当前线程的调度器为this
            t_scheduler = this;

            // 创建根协程，用于在调用者线程中执行调度逻辑
            m_rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
            // 设置线程名称
            Thread::SetName(m_name);

            // 设置调度协程指针
            t_scheduler_fiber = m_rootFiber.get();
            // 记录根线程ID
            m_rootThread = sylar::GetThreadId();
            // 将根线程ID加入线程ID列表
            m_threadIds.push_back(m_rootThread);
        } else {
            // 调用者线程不参与调度，设置根线程ID为-1
            m_rootThread = -1;
        }
        // 设置工作线程数量
        m_threadCount = threads;
    }

    // 协程调度器析构函数
    // 确保调度器已经停止，并清理线程局部变量
    Scheduler::~Scheduler() {
        // 确保调度器已经停止
        SYLAR_ASSERT(m_stopping);
        // 如果当前线程的调度器是this，则清空线程局部变量
        if (GetThis() == this) {
            t_scheduler = nullptr;
        }
    }

    // 获取当前线程的调度器
    // 返回当前线程的调度器指针，如果没有则返回nullptr
    Scheduler* Scheduler::GetThis() {
        return t_scheduler;
    }

    // 获取当前线程的调度协程
    // 返回当前线程的调度协程指针
    // 用于协程切换时确定切换目标
    Fiber* Scheduler::GetMainFiber() {
        return t_scheduler_fiber;
    }

    // 启动协程调度器
    // 创建指定数量的工作线程，每个线程都运行run()函数
    // 如果调度器已经启动则直接返回
    void Scheduler::start() {
        MutexType::Lock lock(m_mutex);
        // 如果调度器已经启动（m_stopping为false），直接返回
        if (!m_stopping) {
            return;
        }
        // 设置调度器为运行状态
        m_stopping = false;
        // 确保线程池为空（防止重复启动）
        SYLAR_ASSERT(m_threads.empty());

        // 调整线程池大小
        m_threads.resize(m_threadCount);
        // 创建工作线程
        for (size_t i = 0; i < m_threadCount; ++i) {
            // 创建线程，绑定run函数作为线程执行函数
            m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this),
                m_name + "_" + std::to_string(i)));
            // 记录线程ID
            m_threadIds.push_back(m_threads[i]->getId());
        }
        lock.unlock();
    }

    // 停止协程调度器
    // 优雅地停止调度器，等待所有任务完成和线程退出
    // 停止过程分为几个阶段：
    // 1. 检查快速停止条件
    // 2. 验证调用线程的合法性
    // 3. 设置停止标志并通知所有线程
    // 4. 处理根协程的剩余工作
    // 5. 等待所有工作线程退出
    void Scheduler::stop() {
        // 设置自动停止标志
        m_autoStop = true;

        // 检查是否满足快速停止条件：
        // 1. 存在根协程（use_caller=true）
        // 2. 没有工作线程（只有根线程参与调度）
        // 3. 根协程已经执行完毕（TERM或INIT状态）
        if (m_rootFiber
                && m_threadCount == 0
                && (m_rootFiber->getState() == Fiber::TERM
                    || m_rootFiber->getState() == Fiber::INIT)) {
            SYLAR_LOG_INFO(g_logger) << this << " stopped";
            m_stopping = true;

            // 如果调度器已经完全停止，直接返回
            if (stopping()) {
                return;
            }
        }

        // 验证stop函数是否在正确的线程中被调用
        if (m_rootThread != -1) {
            // use_caller=true时，必须在调用者线程中调用stop
            SYLAR_ASSERT(GetThis() == this);
        } else {
            // use_caller=false时，不能在工作线程中调用stop
            SYLAR_ASSERT(GetThis() != this);
        }

        // 设置停止标志
        m_stopping = true;
        // 通知所有工作线程停止
        for (size_t i = 0; i < m_threadCount; ++i) {
            tickle();
        }

        // 如果存在根协程，也需要通知
        if (m_rootFiber) {
            tickle();
        }

        // 如果存在根协程且调度器还没有完全停止
        // 让根协程有机会完成剩余工作
        if (m_rootFiber) {
            if (!stopping()) {
                m_rootFiber->call();
            }
        }

        // 等待所有工作线程结束
        std::vector<Thread::ptr> thrs;
        {
            // 在锁保护下转移线程池，避免并发访问
            MutexType::Lock lock(m_mutex);
            thrs.swap(m_threads);
        }

        // 逐个等待线程结束
        for (auto& i : thrs) {
            i->join();
        }
    }

    // 设置当前线程的调度器
    // 将当前线程的调度器指针设置为this
    // 用于在工作线程中建立线程与调度器的关联
    void Scheduler::setThis() {
        t_scheduler = this;
    }

    // 协程调度器的核心调度函数
    // 这是调度器的主要工作函数，运行在每个工作线程中
    // 负责从任务队列中取出任务并执行，实现协程的调度逻辑
    void Scheduler::run() {
        SYLAR_LOG_DEBUG(g_logger) << m_name << " run";

        // 启用hook机制，用于协程化系统调用
        set_hook_enable(true);

        // 设置当前线程的调度器指针
        setThis();

        // 如果当前线程不是根线程(use_caller=false的工作线程)
        // 则设置当前线程的调度协程为当前协程
        if(sylar::GetThreadId() != m_rootThread) {
            t_scheduler_fiber = Fiber::GetThis().get();
        }

        // 创建空闲协程，当没有任务时执行
        Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
        // 回调函数协程，用于复用执行函数类型的任务
        Fiber::ptr cb_fiber;

        // 任务封装结构，用于从任务队列中取出的任务
        FiberAndThread ft;
        // 调度器主循环
        while(true) {
            // 重置任务结构
            ft.reset();
            bool tickle_me = false;  // 是否需要通知其他线程
            bool is_active = false;  // 是否取到了有效任务

            // ========== 任务获取阶段 ==========
            {
                MutexType::Lock lock(m_mutex);
                auto it = m_fibers.begin();

                // 遍历任务队列，寻找可执行的任务
                while(it != m_fibers.end()) {
                    // 检查任务是否指定了特定线程执行
                    if(it->thread != -1 && it->thread != sylar::GetThreadId()) {
                        ++it;
                        tickle_me = true;  // 需要通知其他线程处理
                        continue;
                    }

                    // 确保任务有效（要么是协程，要么是回调函数）
                    SYLAR_ASSERT(it->fiber || it->cb);

                    // 跳过正在执行的协程
                    if(it->fiber && it->fiber->getState() == Fiber::EXEC) {
                        ++it;
                        continue;
                    }

                    // 找到可执行任务，取出并从队列中删除
                    ft = *it;
                    m_fibers.erase(it++);
                    ++m_activeThreadCount;  // 增加活跃线程计数
                    is_active = true;
                    break;
                }
                // 如果还有任务未处理，需要通知其他线程
                tickle_me |= it != m_fibers.end();
            }

            // 通知其他线程有任务需要处理
            if(tickle_me) {
                tickle();
            }

            // ========== 任务执行阶段 ==========

            // 情况1: 执行协程任务
            if(ft.fiber && (ft.fiber->getState() != Fiber::TERM
                            && ft.fiber->getState() != Fiber::EXCEPT)) {
                // 切换到协程执行
                ft.fiber->swapIn();
                --m_activeThreadCount;

                // 根据协程执行后的状态进行处理
                if(ft.fiber->getState() == Fiber::READY) {
                    // 协程让出执行权但还需要继续执行，重新调度
                    schedule(ft.fiber);
                } else if(ft.fiber->getState() != Fiber::TERM
                        && ft.fiber->getState() != Fiber::EXCEPT) {
                    // 协程暂停，设置为HOLD状态
                    ft.fiber->m_state = Fiber::HOLD;
                        }
                ft.reset();
                            }
            // 情况2: 执行回调函数任务
            else if(ft.cb) {
                // 复用cb_fiber或创建新的协程来执行回调函数
                if(cb_fiber) {
                    cb_fiber->reset(ft.cb);
                } else {
                    cb_fiber.reset(new Fiber(ft.cb));
                }
                ft.reset();

                // 切换到回调协程执行
                cb_fiber->swapIn();
                --m_activeThreadCount;

                // 根据回调协程执行后的状态进行处理
                if(cb_fiber->getState() == Fiber::READY) {
                    // 回调协程需要继续执行，重新调度
                    schedule(cb_fiber);
                    cb_fiber.reset();
                } else if(cb_fiber->getState() == Fiber::EXCEPT
                        || cb_fiber->getState() == Fiber::TERM) {
                    // 回调协程异常或结束，清理协程
                    cb_fiber->reset(nullptr);
                        } else {//if(cb_fiber->getState() != Fiber::TERM) {
                            // 回调协程暂停，设置为HOLD状态
                            cb_fiber->m_state = Fiber::HOLD;
                            cb_fiber.reset();
                        }
            }
            // 情况3: 没有任务可执行
            else {
                // 如果之前取到了任务但现在为空，说明任务无效，减少活跃计数
                if(is_active) {
                    --m_activeThreadCount;
                    continue;
                }

                // 检查空闲协程是否已结束，如果结束则退出调度循环
                if(idle_fiber->getState() == Fiber::TERM) {
                    SYLAR_LOG_INFO(g_logger) << "idle fiber term";
                    break;
                }

                // 执行空闲协程，通常会阻塞等待新任务
                ++m_idleThreadCount;
                idle_fiber->swapIn();
                --m_idleThreadCount;

                // 空闲协程执行完毕后，如果没有异常或结束，设置为HOLD状态
                if(idle_fiber->getState() != Fiber::TERM
                        && idle_fiber->getState() != Fiber::EXCEPT) {
                    idle_fiber->m_state = Fiber::HOLD;
                        }
            }
        }
    }

    // 通知调度器有新任务或需要唤醒
    // 基类实现为空，子类可以重写此方法实现具体的通知机制
    // 例如使用信号量、条件变量等同步原语唤醒阻塞的线程
    void Scheduler::tickle() {
        SYLAR_LOG_INFO(g_logger) << "tickle";
    }

    // 判断调度器是否应该停止
    // 返回true表示应该停止，false表示应该继续运行
    // 停止条件：
    // 1. 设置了自动停止标志(m_autoStop)
    // 2. 调度器处于停止状态(m_stopping)
    // 3. 任务队列为空(m_fibers.empty())
    // 4. 没有活跃线程(m_activeThreadCount == 0)
    bool Scheduler::stopping() {
        MutexType::Lock lock(m_mutex);
        return m_autoStop && m_stopping
            && m_fibers.empty() && m_activeThreadCount == 0;
    }

    // 空闲协程执行函数
    // 当没有任务可执行时，线程会执行此函数
    // 在循环中让出执行权，直到调度器停止
    // 子类可以重写此方法实现自定义的空闲逻辑
    void Scheduler::idle() {
        SYLAR_LOG_INFO(g_logger) << "idle";
        // 循环让出执行权，直到调度器停止
        while(!stopping()) {
            sylar::Fiber::YieldToHold();
        }
    }

    // 将当前协程切换到指定线程执行
    // thread: 目标线程ID，-1表示任意线程
    // 将当前协程调度到指定线程执行，当前协程会让出执行权
    // 如果目标线程就是当前线程，则直接返回不做切换
    void Scheduler::switchTo(int thread) {
        // 确保当前线程有调度器
        SYLAR_ASSERT(Scheduler::GetThis() != nullptr);
        // 如果当前调度器就是this
        if (Scheduler::GetThis() == this) {
            // 如果目标线程是任意线程或当前线程，直接返回
            if (thread == -1 || thread == sylar::GetThreadId()) {
                return;
            }
        }
        // 将当前协程调度到指定线程
        schedule(Fiber::GetThis(), thread);
        // 让出当前协程的执行权
        Fiber::YieldToHold();
    }

    // 将调度器状态信息输出到流
    // os: 输出流
    // 返回输出流的引用
    // 输出调度器的基本信息，包括名称、线程数量、活跃线程数等
    // 用于调试和监控调度器状态
    std::ostream& Scheduler::dump(std::ostream& os) {
        os << "[Scheduler name=" << m_name
           << " size=" << m_threadCount
           << " active_count=" << m_activeThreadCount
           << " idle_count=" << m_idleThreadCount
           << " stopping=" << m_stopping
           << " ]" << std::endl
           << "    [threads: ";
        // 输出所有线程ID
        for (size_t i = 0; i < m_threadIds.size(); ++i) {
            if (i) {
                os << ",";
            }
            os << m_threadIds[i];
        }
        os << "]" << std::endl;
        return os;
    }

    // 调度器切换器构造函数
    // target: 目标调度器，如果为nullptr则不进行切换
    // RAII风格的调度器切换器，构造时切换到目标调度器
    // 析构时自动切换回原调度器，确保调度器切换的安全性
    SchedulerSwitcher::SchedulerSwitcher(Scheduler* target) {
        // 保存当前调度器
        m_caller = Scheduler::GetThis();
        // 如果目标调度器存在，切换到目标调度器
        if (target) {
            target->switchTo();
        }
    }

    // 调度器切换器析构函数
    // 自动切换回构造时保存的调度器，实现RAII管理
    SchedulerSwitcher::~SchedulerSwitcher() {
        // 如果保存的调度器存在，切换回去
        if (m_caller) {
            m_caller->switchTo();
        }
    }
}
