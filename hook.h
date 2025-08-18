/**
 * Hook系统 - 系统调用拦截与协程化
 *
 * 功能说明：
 * Hook系统通过函数指针和动态链接技术，拦截标准的阻塞系统调用，
 * 将其替换为协程友好的非阻塞实现，从而在不修改应用代码的情况下，
 * 实现高并发的协程调度。
 *
 * 工作原理：
 * 1. 保存原始系统调用函数的地址到函数指针变量中
 * 2. 重新定义同名的系统调用函数，实现自定义逻辑
 * 3. 在自定义函数中根据hook状态选择执行路径：
 *    - 启用hook：使用协程版本的非阻塞实现
 *    - 禁用hook：调用原始的系统调用函数
 *
 * 支持的系统调用类别：
 * - 睡眠函数：sleep, usleep, nanosleep
 * - 套接字操作：socket, connect, accept
 * - 读操作：read, readv, recv, recvfrom, recvmsg
 * - 写操作：write, writev, send, sendto, sendmsg
 * - 文件控制：close, fcntl, ioctl
 * - 套接字选项：getsockopt, setsockopt
 *
 * 使用场景：
 * - 高并发网络服务器
 * - 需要大量IO操作的应用
 * - 希望无缝迁移到协程的现有代码
 */

//
// Created by admin on 2025/8/18.
//

#ifndef HOOK_H
#define HOOK_H

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

namespace sylar {

    /**
     * @brief 检查当前线程是否启用了hook
     * @return true表示启用hook，false表示禁用hook
     * @note 每个线程都有独立的hook状态，支持线程级别的控制
     */
    bool is_hook_enable();

    /**
     * @brief 设置当前线程的hook状态
     * @param flag true启用hook，false禁用hook
     * @note 线程局部设置，不影响其他线程的hook状态
     */
    void set_hook_enable(bool flag);
}

/**
 * C语言链接规范声明区域
 *
 * 这里声明了所有被hook的系统调用的函数指针类型和变量。
 * 使用extern "C"确保与C标准库的正确链接，避免C++名称修饰。
 *
 * 命名规范：
 * - 函数指针类型：原函数名 + "_fun"
 * - 函数指针变量：原函数名 + "_f"
 *
 * 例如：sleep函数 -> sleep_fun类型 -> sleep_f变量
 */
extern "C" {

    /* ========== sleep相关函数 ========== */

    /** sleep函数指针类型：秒级睡眠 */
    typedef unsigned int (*sleep_fun)(unsigned int seconds);
    /** 保存原始sleep函数地址的指针变量 */
    extern sleep_fun sleep_f;

    /** usleep函数指针类型：微秒级睡眠 */
    typedef int (*usleep_fun)(useconds_t usec);
    /** 保存原始usleep函数地址的指针变量 */
    extern usleep_fun usleep_f;

    /** nanosleep函数指针类型：纳秒级睡眠 */
    typedef int (*nanosleep_fun)(const struct timespec *req, struct timespec *rem);
    /** 保存原始nanosleep函数地址的指针变量 */
    extern nanosleep_fun nanosleep_f;

    /* ========== 套接字相关函数 ========== */

    /** socket函数指针类型：创建套接字 */
    typedef int (*socket_fun)(int domain, int type, int protocol);
    /** 保存原始socket函数地址的指针变量 */
    extern socket_fun socket_f;

    /** connect函数指针类型：连接到远程地址 */
    typedef int (*connect_fun)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    /** 保存原始connect函数地址的指针变量 */
    extern connect_fun connect_f;

    /** accept函数指针类型：接受连接 */
    typedef int (*accept_fun)(int s, struct sockaddr *addr, socklen_t *addrlen);
    /** 保存原始accept函数地址的指针变量 */
    extern accept_fun accept_f;

    /* ========== 读操作相关函数 ========== */

    /** read函数指针类型：从文件描述符读取数据 */
    typedef ssize_t (*read_fun)(int fd, void *buf, size_t count);
    /** 保存原始read函数地址的指针变量 */
    extern read_fun read_f;

    /** readv函数指针类型：向量化读取（scatter-gather I/O） */
    typedef ssize_t (*readv_fun)(int fd, const struct iovec *iov, int iovcnt);
    /** 保存原始readv函数地址的指针变量 */
    extern readv_fun readv_f;

    /** recv函数指针类型：从套接字接收数据 */
    typedef ssize_t (*recv_fun)(int sockfd, void *buf, size_t len, int flags);
    /** 保存原始recv函数地址的指针变量 */
    extern recv_fun recv_f;

    /** recvfrom函数指针类型：从套接字接收数据并获取发送方地址 */
    typedef ssize_t (*recvfrom_fun)(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
    /** 保存原始recvfrom函数地址的指针变量 */
    extern recvfrom_fun recvfrom_f;

    /** recvmsg函数指针类型：接收消息（支持辅助数据） */
    typedef ssize_t (*recvmsg_fun)(int sockfd, struct msghdr *msg, int flags);
    /** 保存原始recvmsg函数地址的指针变量 */
    extern recvmsg_fun recvmsg_f;

    /* ========== 写操作相关函数 ========== */

    /** write函数指针类型：向文件描述符写入数据 */
    typedef ssize_t (*write_fun)(int fd, const void *buf, size_t count);
    /** 保存原始write函数地址的指针变量 */
    extern write_fun write_f;

    /** writev函数指针类型：向量化写入（scatter-gather I/O） */
    typedef ssize_t (*writev_fun)(int fd, const struct iovec *iov, int iovcnt);
    /** 保存原始writev函数地址的指针变量 */
    extern writev_fun writev_f;

    /** send函数指针类型：向套接字发送数据 */
    typedef ssize_t (*send_fun)(int s, const void *msg, size_t len, int flags);
    /** 保存原始send函数地址的指针变量 */
    extern send_fun send_f;

    /** sendto函数指针类型：向指定地址发送数据 */
    typedef ssize_t (*sendto_fun)(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
    /** 保存原始sendto函数地址的指针变量 */
    extern sendto_fun sendto_f;

    /** sendmsg函数指针类型：发送消息（支持辅助数据） */
    typedef ssize_t (*sendmsg_fun)(int s, const struct msghdr *msg, int flags);
    /** 保存原始sendmsg函数地址的指针变量 */
    extern sendmsg_fun sendmsg_f;

    /* ========== 文件控制相关函数 ========== */

    /** close函数指针类型：关闭文件描述符 */
    typedef int (*close_fun)(int fd);
    /** 保存原始close函数地址的指针变量 */
    extern close_fun close_f;

    /** fcntl函数指针类型：文件控制操作（可变参数） */
    typedef int (*fcntl_fun)(int fd, int cmd, ... /* arg */ );
    /** 保存原始fcntl函数地址的指针变量 */
    extern fcntl_fun fcntl_f;

    /** ioctl函数指针类型：设备控制操作（可变参数） */
    typedef int (*ioctl_fun)(int d, unsigned long int request, ...);
    /** 保存原始ioctl函数地址的指针变量 */
    extern ioctl_fun ioctl_f;

    /* ========== 套接字选项相关函数 ========== */

    /** getsockopt函数指针类型：获取套接字选项 */
    typedef int (*getsockopt_fun)(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
    /** 保存原始getsockopt函数地址的指针变量 */
    extern getsockopt_fun getsockopt_f;

    /** setsockopt函数指针类型：设置套接字选项 */
    typedef int (*setsockopt_fun)(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
    /** 保存原始setsockopt函数地址的指针变量 */
    extern setsockopt_fun setsockopt_f;

    /* ========== 扩展功能函数 ========== */

    /**
     * @brief 带超时的连接函数
     * @param fd 套接字文件描述符
     * @param addr 目标地址
     * @param addrlen 地址长度
     * @param timeout_ms 超时时间（毫秒）
     * @return 连接结果，0表示成功，-1表示失败
     * @note 这是sylar框架提供的扩展函数，支持超时控制的连接操作
     */
    extern int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen, uint64_t timeout_ms);
}

#endif //HOOK_H
