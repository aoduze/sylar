//
// Created by admin on 2025/8/5.
//

#include "fiber.h"
#include "config.h"
#include "log.h"
#include "macro.h"
#include <atomic>

// 个人觉得还是把流程图跟要点写下来
// 不写的话,再过三天连我自己都看不懂了

/**
 * 1. 为什么主线程构造函数是私有的？
 *    我们只能通过GetThis()来懒加载主协程
 *    不需要分配栈空间的,因为我们主协程是加载在线程原有的栈中
 *
 * 2. 为什么析构函数是私有的？
 *    通过智能指针管理,不允许直接delete
 *    析构时必须确认已经没有正在执行的协程
 *
 *    把一些我觉得重要的细节写一下
 *    1. 时刻关注生命周期, 适当运用智能指针与延长生命周期
 *    要注意swapcontext中link的随时指向
 *    当然,我们约定好的配置不会出错,但需要警惕
 */
namespace sylar {
    // 当前协程id
    static std::atomic<uint64_t> s_fiber_id {0};
    // 全局计数
    static std::atomic<uint64_t> s_fiber_count {0};

    // 线程局部变量(当前协程)
    static thread_local Fiber* t_fiber = nullptr;
    // 线程指针(主协程智能指针)
    static thread_local Fiber::ptr t_threadFiber = nullptr;

    // 配置指定协程的 栈大小
    static ConfigVar<uint32_t>::ptr g_fiber_stack_size =
        Config::Lookup<uint32_t>("fiber.stack_size",128 * 1024, "fiber stack size");

    //我们想要统一管理栈大小
    class MallocStackAllocator {
    public:
        // 分配协程栈
        static void* Alloc(size_t size) {
            return malloc(size);
        }

        // 销毁协程栈
        // 某些内存管理器需要知道释放的内存块的大小
        static void Dealloc(void* vp, size_t size) {
            return free(vp);
        }
    };

    using StackAllocator = MallocStackAllocator;

    uint64_t Fiber::GetFiberId() {
        if (t_fiber) {
            return t_fiber->getId();
        }
        return 0;
    }

    Fiber::Fiber() {
        m_state = EXEC;
        //设置协程指针为当前协程
        SetThis(this);

        if (getcontext(&m_ctx)) {
            SYLAR_ASSERT(false, "getcontext");
        }

        ++s_fiber_count;

        SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber main";
    }

    Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool use_caller)
        : m_id(++s_fiber_count)
        , m_cb(cb) {
        ++s_fiber_count;
        //栈大小
        m_stacksize = stacksize ? stacksize : g_fiber_stack_size->getValue();

        //分配栈内存
        m_stack = StackAllocator::Alloc(m_stacksize);
        //获取当前上下文
        if (getcontext(&m_ctx)) {
            SYLAR_ASSERT(false, "getcontext");
        }
        //没有关联上下文(如果有关联上下文, 自身结束时会回到关联上下文执行)

        //指定上下文起始指针
        m_ctx.uc_link = nullptr;
        //指定栈大小
        m_ctx.uc_stack.ss_sp = m_stack;
        //创建上下文
        m_ctx.uc_stack.ss_size = m_stacksize;

        if (!use_caller) {
            makecontext(&m_ctx, &Fiber::MainFunc, 0);
        } else {
            makecontext(&m_ctx, &Fiber::CallerMainFunc, 0);
        }

        SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber id=" << m_id;
    }

    Fiber::~Fiber() {
        --s_fiber_count;
        if (m_stack) {
            //子线程会有独属的m_stack
            SYLAR_ASSERT(m_state == TERM || m_state == INIT);
            //回收栈
            StackAllocator::Dealloc(m_stack,m_stacksize);
        } else {
            SYLAR_ASSERT(!m_cb);
            SYLAR_ASSERT(m_state == EXEC);

            Fiber* cur = t_fiber;
            if (cur == this) {
                SetThis(nullptr);
            }
        }
    }

    //重置协程函数, 并重置状态, 可以复用栈
    void Fiber::reset(std::function<void()> cb) {
        SYLAR_ASSERT(m_stack);
        SYLAR_ASSERT(m_state == TERM
                || m_state == INIT
                || m_state == EXCEPT);
        m_cb = cb;
        if (getcontext(&m_ctx)) {
            SYLAR_ASSERT(false, "getcontext");
        }

        m_ctx.uc_link = nullptr;
        m_ctx.uc_stack.ss_sp = m_stack;
        m_ctx.uc_stack.ss_size = m_stacksize;

        makecontext(&m_ctx, &Fiber::MainFunc, 0);
        m_state = INIT;
    }

    // 将当前线程切换到执行状态并执行当前线程的主协程
    void Fiber::call() {
        SetThis(this);
        m_state = EXEC;
        if (swapcontext(&t_threadFiber->m_ctx, &m_ctx)) {
            SYLAR_ASSERT(false, "swapcontext");
        }
    }

    // 切换到主协程,子协程任务退至后台
    void Fiber::back() {
        SetThis(t_threadFiber.get());
        if(swapcontext(&m_ctx, &t_threadFiber->m_ctx)) {
            SYLAR_ASSERT(false, "swapcontext");
        }
    }

    // 切换到当前线程执行
    void Fiber::swapIn() {
        SetThis(this);
        SYLAR_ASSERT(m_state != EXEC);
        m_state = EXEC;
        if (swapcontext(&t_threadFiber->m_ctx, &m_ctx)) {
            SYLAR_ASSERT(false, "swapcontext");
        }
    }

    // 切换到主协程
    void Fiber::swapOut() {
        SetThis(Scheduler::GetMainFiber());
        if (swapcontext(&m_ctx, &t_threadFiber->m_ctx)) {
            SYLAR_ASSERT(false, "swapcontext");
        }
    }

    // 设置当前协程
    void Fiber::SetThis(Fiber* f) {
        t_fiber = f;
    }

    // 返回当前协程
    Fiber::ptr Fiber::GetThis() {
        if (t_fiber) {
            return t_fiber->shared_from_this();
        }

        // 需要理解的是,主协程并非是跟着构造函数走的
        // 一样的是按需创建,所以我们这里创建了主协程
        // 符合我们的懒加载设计,非常的牛逼啊兄弟
        Fiber::ptr main_fiber(new Fiber);
        SYLAR_ASSERT(t_fiber == main_fiber.get());
        t_threadFiber == main_fiber;
        return t_fiber -> shared_from_this();
    }

    // 将协程切换到Ready状态,并放到后台
    void Fiber::YieldToReady() {
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur->m_state == EXEC);
        cur -> m_state = READY;
        cur -> swapOut();
    }

    // 将协程切换到Hold状态,并放到后台
    void Fiber::YieldToHold() {
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur->m_state == EXEC);
        cur -> m_state = HOLD;
        cur -> swapOut();
    }

    // 总协程数
    uint64_t Fiber::TotalFibers() {
        return s_fiber_count;
    }

    void Fiber::MainFunc() {
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur);
        try {
            /* 有点瞎,一直在找cb,重点注释一下
             * 执行完毕以后,我们立刻清空当前cb
             * 并把状态设置为终止,非常的谨慎啊兄弟们
             */
            cur -> m_cb();
            cur -> m_cb = nullptr;
            cur -> m_state = TERM;
        } catch (...) {
            cur -> m_state = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except"
                << " fiber_id=" << cur->getId()
                << std::endl
                << sylar::BacktraceToString();
        }

        // 协程对象可能被调度器或者其他地方引用,如果swapOut之前不检查
        // 这是非常危险的操作,所以我们获取原始的智能指针并释放减少引用技术
        auto raw_ptr = cur.get();
        cur.reset();
        raw_ptr -> swapOut();

        SYLAR_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->getId()));
    }

    // 协程执行函数,执行完成回到线程调度协程
    void Fiber::CallerMainFunc() {
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur);
        try {
            cur->m_cb();
            cur->m_cb = nullptr;
            cur->m_state = TERM;
        } catch (std::exception& ex) {
            cur->m_state = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
                << " fiber_id=" << cur->getId()
                << std::endl
                << sylar::BacktraceToString();
        } catch (...) {
            cur->m_state = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except"
                << " fiber_id=" << cur->getId()
                << std::endl
                << sylar::BacktraceToString();
        }

        auto raw_ptr = cur.get();
        cur.reset();
        raw_ptr->back();
        SYLAR_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->getId()));
    }
}

