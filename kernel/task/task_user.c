//
// Created by toney on 9/29/23.
//

#include "../../include/task.h"
#include "../../include/syscall.h"

/* 模拟用户进程A */
void *user_task_a(void *args) {
    uint32 p1 = syscall(SYS_ALLOC_PAGE);
    syscall2(SYS_PRINT, "U_PA : p1 = 0x%x\n", p1);
    syscall1(SYS_FREE_PAGE, p1);

    uint32 *p2 = (uint32 *) syscall(SYS_ALLOC_PAGE);
    *p2 = 100;
    syscall3(SYS_PRINT, "U_PA : p2 [0x%x -> 0x%x]\n", p2, *p2);

    for (int i = 0; i < 100; ++i) {
        int pid = syscall(SYS_GET_PID);
    }

    syscall1(SYS_PRINT, "U_PA : ready exit ~\n");
    syscall(SYS_EXIT);
    return NULL;
}

/* 模拟用户进程B */
void *user_task_b(void *args) {
    uint32 p1 = syscall(SYS_ALLOC_PAGE);
    syscall2(SYS_PRINT, "U_PB ::: p1 = 0x%x\n", p1);

    uint32 p2 = syscall(SYS_ALLOC_PAGE);
    syscall2(SYS_PRINT, "U_PB ::: p2 = 0x%x\n", p2);

    syscall1(SYS_PRINT, "U_PB ::: ready exit ~\n");
    syscall(SYS_EXIT);
    return NULL;
}
