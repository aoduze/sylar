//
// Created by admin on 2025/8/11.
//

/**
 * 定时器执行流程说明：
 *
 * 1. 定时器创建与注册
 *    - 用户调用 TimerManager::addTimer() 创建定时器
 *    - 定时器按到期时间有序存储在 std::set<Timer::ptr> m_timers 中
 *    - 支持一次性定时器和循环定时器两种类型
 *
 * 2. 定时器监控与检测
 *    - IOManager::idle() 主事件循环中调用 TimerManager::getNextTimer()
 *    - 获取距离下一个定时器触发的时间间隔
 *    - 该时间用作 epoll_wait() 的超时参数，确保定时器能及时触发
 *
 * 3. 定时器到期处理
 *    - 当 epoll_wait() 超时或有IO事件时，调用 TimerManager::listExpiredCb()
 *    - 检测系统时钟是否回拨，确保时间的准确性
 *    - 找出所有已到期的定时器，提取其回调函数
 *    - 将回调函数提交给调度器执行
 *
 * 4. 定时器类型处理
 *    - 一次性定时器：执行后清理资源，自动销毁
 *    - 循环定时器：重新计算下次触发时间，重新插入定时器集合
 *
 * 5. 时钟安全机制
 *    - detectClockRollover() 检测系统时钟回拨（超过1小时的时间倒退）
 *    - 时钟回拨时重新处理所有定时器，确保系统稳定性
 *
 * 6. 性能优化设计
 *    - 使用有序集合 std::set 存储定时器，查找效率 O(log n)
 *    - 双重锁检查模式，减少写锁竞争
 *    - 批量处理到期定时器，减少系统调用
 *    - 内存预分配，避免频繁的内存重分配
 */

#include "timer.h"
#include "util.h"

namespace sylar {

    // 定时器比较大小
    bool Timer::Comparator::operator()(const Timer::ptr& lhs
        ,const Timer::ptr& rhs) const {

        if (!lhs && !rhs) { return false;}
        if (!lhs) { return true;}
        if (!rhs) { return false;}
        if (lhs -> m_next < rhs -> m_next) { return true;}
        if (lhs -> m_next > rhs -> m_next) { return false;}

        return lhs.get() < rhs.get();
    }

    Timer::Timer(uint64_t ms, std::function<void()> cb,
             bool recurring, TimerManager* manager)
    :m_recurring(recurring)
    ,m_ms(ms)
    ,m_cb(cb)
    ,m_manager(manager) {
        m_next = sylar::GetCurrentMS() + m_ms;
    }

    Timer::Timer(uint64_t next)
    :m_next(next) {
    }

    bool Timer::cancel() {
        TimerManager::RWMutexType::WriteLock lock(m_manager -> m_mutex);
        if (m_cb) {
            m_cb = nullptr;
            auto it = m_manager -> m_timers.find(shared_from_this());
            if (it != m_manager -> m_timers.end()) {
                m_manager -> m_timers.erase(it);
            }
            return true;
        }
        return false;
    }

    bool Timer::refresh() {
        TimerManager::RWMutexType::WriteLock lock(m_manager -> m_mutex);
        if (!m_cb) {
            return false;
        }

        // 如果当前定时器依旧
        auto it = m_manager -> m_timers.find(shared_from_this());
        if (it == m_manager -> m_timers.end()) {
            return false;
        }
        m_manager -> m_timers.erase(it);
        m_next = sylar::GetCurrentMS() + m_ms;
        m_manager -> m_timers.insert(shared_from_this());
        return true;
    }

    /**
     * 这里我们先删除再添加是最好的处理方式
     * 为了防止错误的残留信息
     */
    bool Timer::reset(uint64_t ms, bool from_now) {
        if (ms == m_ms && !from_now) {
            return false;
        }
        TimerManager::RWMutexType::WriteLock lock(m_manager -> m_mutex);
        if (!m_cb) {
            return false;
        }
        auto it = m_manager -> m_timers.find(shared_from_this());
        if (it == m_manager -> m_timers.end()) {
            return false;
        }
        m_manager -> m_timers.erase(it);
        uint64_t start = 0;
        if (from_now) {
            start = sylar::GetCurrentMS();
        } else {
            start = m_next - m_ms;
        }
        m_ms = ms;
        m_next = start + m_ms;
        m_manager -> m_timers.insert(shared_from_this());
        return true;
    }

    TimerManager::TimerManager() {
        // m_previouseTime为上次执行时间
        m_previouseTime = sylar::GetCurrentMS();
    }

    TimerManager::~TimerManager() {}

    Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb
        , bool recurring) {
        Timer::ptr timer(new Timer(ms, cb, recurring, this));
        RWMutexType::WriteLock lock(m_mutex);
        addTimer(timer, lock);
        return timer;
    }

    // 条件定时器的回调包装器
    static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
        std::shared_ptr<void> tmp = weak_cond.lock();
        if(tmp) {
            cb();
        }
    }

    Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb
                                        ,std::weak_ptr<void> weak_cond
                                        ,bool recurring) {
        return addTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
    }

    uint64_t TimerManager::getNextTimer() {
        RWMutexType::ReadLock lock(m_mutex);
        m_tickled = false;
        if (m_timers.empty()) {
            return ~0ull;
        }

        // 如果当前时间已经大于到期时间了
        // 返回0, 代表立即需要处理
        const Timer::ptr& next = *m_timers.begin();
        uint64_t now_ms = sylar::GetCurrentMS();
        if (now_ms >= next -> m_next) { return 0; }

        // 返回距离下一个定时器触发的毫秒数
        return next -> m_next - now_ms;
    }


    void TimerManager::listExpiredCb(std::vector<std::function<void()>>& cbs) {
        uint64_t now_ms = sylar::GetCurrentMS();
        // 用于存放到期的定时器
        std::vector<Timer::ptr> expired;
        {
            RWMutexType::ReadLock lock(m_mutex);
            if (m_timers.empty()) {
                return;
            }
        }

        RWMutexType::WriteLock lock(m_mutex);
        if (m_timers.empty()) {
            return;
        }

        // 因为我们是set集合,如果首个定时器都没有到期的话
        // 后面也就都不用检查了, 直接返回即可
        bool rollover = detectClockRollover(now_ms);
        if (!rollover && (*m_timers.begin()) -> m_next > now_ms) {
            return;
        }

        Timer::ptr now_timer(new Timer(now_ms));
        auto it = rollover ? m_timers.end() : m_timers.lower_bound(now_timer);
        // 跳过相同时间的定时器
        while (it != m_timers.end() && (*it) -> m_next == now_ms) {
            ++it;
        }

        // 我们把从头开始的所有相同或过期的定时器放到expired中
        expired.insert(expired.begin(), m_timers.begin(), it);
        m_timers.erase(m_timers.begin(), it);
        // 预先分配内存
        cbs.reserve(expired.size());

        for(auto& timer : expired) {
            // 将过期定时器的回调添加到cbs中
            cbs.push_back(timer->m_cb);
            // 如果是循环定时器的话
            // 重新计算到期时间,并重新添加到m_timers中
            if(timer->m_recurring) {
                timer->m_next = now_ms + timer->m_ms;
                m_timers.insert(timer);
            } else {
                timer->m_cb = nullptr;
            }
        }
    }

    void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock& lock) {
        auto it = m_timers.insert(val).first;
        bool at_front = (it == m_timers.begin()) && !m_tickled;
        if(at_front) {
            m_tickled = true;
        }
        lock.unlock();

        if(at_front) {
            onTimerInsertedAtFront();
        }
    }

    // 检测系统时钟是否发生了回拨
    bool TimerManager::detectClockRollover(uint64_t now_ms) {
        bool rollover = false;
        // 当前时间小于上次记录的时间
        // 时间差超过一小时
        if(now_ms < m_previouseTime &&
                now_ms < (m_previouseTime - 60 * 60 * 1000)) {
            rollover = true;
                }
        m_previouseTime = now_ms;
        return rollover;
    }

    // 检测是否有次定时器
    bool TimerManager::hasTimer() {
        RWMutexType::ReadLock lock(m_mutex);
        return !m_timers.empty();
    }

}