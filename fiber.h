//
// Created by admin on 2025/8/5.
//

#ifndef FIBER_H
#define FIBER_H

//先导知识
//在Cpp中,指针的大小与指向类型的大小是一样的

#include <ucontext.h>
#include <execinfo.h>
#include <memory>
#include <functional>

namespace sylar {
    class Scheduler;

    // 协程类
    class Fiber : public std::enable_shared_from_this<Fiber> {
    friend class Scheduler;
    public:
        typedef std::shared_ptr<Fiber> ptr;

        enum State {
            /// 初始化
            INIT,
            /// 暂停状态
            HOLD,
            /// 执行中状态
            EXEC,
            /// 结束状态
            TERM,
            /// 可执行状态
            READY,
            /// 异常状态
            EXCEPT
        };
    private:
        // 无参构造函数
        // 每个线程第一个协程的构造
        Fiber();

    public:
        //构造函数
        Fiber(std::function<void()> cb, size_t stacksize = 0, bool use_caller = false);

        // 析构
        ~Fiber();

        //重置协程状态为INIT
        void reset(std::function<void()> cb);

        // 切换到当前协程执行
        void swapIn();

        // 切换到后台执行
        void swapOut();

        // 将当前线程切换到执行状态
        // 执行的为当前线程的主协程
        void call();

        // 将当前线程切换到后台
        // 执行的为该协程
        void back();

        // 返回协程id
        uint64_t getId() const { return m_id; }

        // 返回协程状态
        State getState() const { return m_state; }

    public:
        //设置当前协程的运行协程
        static void SetThis(Fiber* f);

        //返回当前所在协程
        static Fiber::ptr GetThis();

        //将当前协程切换为后台,并设置为Ready状态
        static void YieldToReady();

        // 将当前协程切换为后台,并设置为Hold状态
        static void YieldToHold();

        // 返回当前协程的总数量
        static uint64_t TotalFibers();

        // 协程执行函数
        static void MainFunc();

        //执行函数
        static void CallerMainFunc();

        //获取协程ID
        static uint64_t GetFiberId();
    private:
        //协程id
        uint64_t m_id = 0;
        //协程运行栈大小
        uint32_t m_stacksize = 0;
        //协程状态
        State m_state = INIT;
        //协程上下文
        ucontext_t m_ctx;
        //当前栈指针
        void* m_stack = nullptr;
        //回调
        std::function<void()> m_cb;
    };

}
#endif //FIBER_H
