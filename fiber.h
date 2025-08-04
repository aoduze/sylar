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

    //携程类
    class Fiber : public std::enable_shared_from_this<Fiber> {

    };

}
#endif //FIBER_H
