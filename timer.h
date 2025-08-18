//
// Created by admin on 2025/8/11.
//

#ifndef TIMER_H
#define TIMER_H

#include <memory>
#include <vector>
#include <set>
#include "Thread.h"

namespace sylar {

    class TimerManager;

    class Timer : public std::enable_shared_from_this<Timer> {
        friend class TimerManager;
    public:
        typedef std::shared_ptr<Timer> ptr;

        // 取消定时器
        bool cancel();

        // 刷新定时器
        bool refresh();

        // 重置定时器时间
        bool reset(uint64_t ms, bool from_now);
    private:
        // ms为定时器间隔时间
        // cb回调函数
        // recurring是否循环？
        // manager 定时器管理器
        Timer(uint64_t ms, std::function<void()> cb,
            bool recurring, TimerManager* manager);
        // 间隔时间
        Timer(uint64_t next);
    private:
        // 是否循环定时器
        bool m_recurring = false;
        // 执行周期
        uint64_t m_ms = 0;
        // 精确的执行时间
        uint64_t m_next = 0;
        // 回调函数
        std::function<void()> m_cb;
        // 定时器管理类指针
        TimerManager* m_manager = nullptr;
    private:
        struct Comparator {
            // 比较定时器的智能指针的大小(当然, 按照智能指针执行的)
            bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs) const;
        };
    };

    class TimerManager {
        friend class Timer;
    public:
        typedef RWMutex RWMutexType;

        TimerManager();

        virtual ~TimerManager();

        Timer::ptr addTimer(uint64_t, std::function<void()>, bool);
        Timer::ptr addConditionTimer(uint64_t, std::function<void()>,
            std::weak_ptr<void>, bool);

        // 离当前最近的一个定时器执行的时间间隔
        uint64_t getNextTimer();

        // 获取需要执行的定时器的回调函数列表
        void listExpiredCb(std::vector<std::function<void()> >& cb);

        // 是否有定时器
        bool hasTimer();
    protected:
        // 新定时器添加到首部并执行
        virtual void onTimerInsertedAtFront() = 0;

        // 将定时器添加到管理器中
        bool addTimer(Timer::ptr val ,RWMutexType::WriteLock& lock);
    private:
        // 检测服务器时间是否正确
        bool detectClockRollover(uint64_t now_ms);
    private:
        RWMutexType m_mutex;
        // 定时器集合
        std::set<Timer::ptr, Timer::Comparator> m_timers;
        // 是否触发ontimerInsertedAtFront
        bool m_tickled = false;
        // 上次执行时间
        uint64_t m_previouseTime = 0;
    };
}

#endif //TIMER_H
