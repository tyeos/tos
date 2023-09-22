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
    SYS_PRINT,   // 0
    SYS_GET_PID  // 1
};

void syscall_init(void);

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
