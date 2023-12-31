//
// Created by Toney on 2023/9/22.
//

#ifndef TOS_SYSCALL_H
#define TOS_SYSCALL_H

/*
 * 最大支持的系统调用子功能个数
 */
#define SYSCALL_NR_MAX 32

/*
 * 系统调用子功能号
 */
enum SYSCALL_NUMBER {
    SYS_PRINT,      // 0, 控制台打印
    SYS_GET_PID,    // 1, 获取进程号
    SYS_EXIT,       // 2, 退出当前进程
    SYS_ALLOC_PAGE, // 3, 分配物理页
    SYS_FREE_PAGE   // 4, 释放物理页
};

void syscall_init();

/*
 * 无参数的系统调用
 */
#define syscall(NUMBER) ({ \
    int retval;             \
    asm volatile (          \
        "int 0x80"          \
        : "=a" (retval)     \
        : "a" (NUMBER)      \
        : "memory"          \
    );                      \
    retval;                 \
})

/*
 * 3个参数的系统调用
 */
#define syscall3(NUMBER, ARG1, ARG2, ARG3) ({ \
    int retval;                                \
    asm volatile (                             \
        "int 0x80"                             \
        : "=a" (retval)                        \
        : "a" (NUMBER), "b" (ARG1), "c" (ARG2), "d" (ARG3) \
        : "memory"                             \
    );                                         \
    retval;                                    \
})

#define syscall1(NUMBER, ARG1) syscall3(NUMBER, ARG1, 0, 0)
#define syscall2(NUMBER, ARG1, ARG2) syscall3(NUMBER, ARG1, ARG2, 0)

#endif //TOS_SYSCALL_H
