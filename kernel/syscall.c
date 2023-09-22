//
// Created by toney on 23-9-20.
//

#include "../include/syscall.h"
#include "../include/print.h"
#include "../include/task.h"

void *syscall_table[SYSCALL_NR_MAX];

void syscall_init(void) {
    syscall_table[SYS_PRINT] = printk;
    syscall_table[SYS_GET_PID] = get_current_task_pid;
}

