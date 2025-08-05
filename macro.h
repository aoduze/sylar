//
// Created by admin on 2025/8/6.
//

#ifndef MACRO_H
#define MACRO_H

#include <string.h>
#include <assert.h>
#include "log.h"
#include "util.h"

//编译器优化提示
#if defined __GNUC__ || defined __llvm__
/// LIKCLY 宏的封装
# define SYLAR_LIKELY(x)      __builtin_expect(!!(x), 1)
/// UNLIKE 宏的封装
# define SYLAR_UNLIKELY(x)      __builtin_expect(!!(x), 0)
#else
//如果是 llvm编译器的话
# define SYLAR_LIKELY(x)      (x)
# define SYLAR_UNLIKE(x)      (x)
#endif

// 断言宏封装
#define SYLAR_ASSERT(x) \
    if(SYLAR_UNLIKELY(!(x))) { \
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION: " #x \
            << "\nbacktrace:\n " \
            << sylar::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

//断言宏封装
#define SYLAR_ASSERT2(x, w) \
    if(SYLAR_UNLIKELY(!(x))) { \
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION " #x \
            << "\n" << w \
            << "\nbacktrace:\n " \
            << sylar::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#endif //MACRO_H
