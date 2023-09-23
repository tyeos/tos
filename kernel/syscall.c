//
// Created by toney on 23-9-20.
//

#include "../include/syscall.h"
#include "../include/print.h"
#include "../include/task.h"

extern void exit_user_model(); // 退出当前用户任务函数，汇编中定义

void *syscall_table[SYSCALL_NR_MAX];

void syscall_init(void) {
    syscall_table[SYS_PRINT] = printk;
    syscall_table[SYS_GET_PID] = get_current_task_pid;
    syscall_table[SYS_EXIT] = exit_user_model;
}

