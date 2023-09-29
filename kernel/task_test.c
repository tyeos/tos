//
// Created by toney on 9/29/23.
//

#include "../include/task.h"
#include "../include/print.h"
#include "../include/sys.h"
#include "../include/chain.h"
#include "../include/bitmap.h"
#include "../include/string.h"
#include "../include/syscall.h"
#include "../include/lock.h"
#include "../include/ide.h"


#define TEST_READ_SEC_CNT 2
char buff[TEST_READ_SEC_CNT << 9] = {0};


/* 模拟内核任务A */
void *kernel_task_a(void *args) {

    ide_read(get_disk(0, 0), 0, buff, TEST_READ_SEC_CNT);
    for (int i = 0; i < TEST_READ_SEC_CNT << 9; ++i) {
        if (i % 16 == 0) printk("\n");
        printk("%02x ", buff[i] & 0xff);
    }
    printk("\ntest ide read end~ \n");


    for (int i = 0; i < 10; ++i) {
        printk("K_A ~ %d\n", i);
        HLT
    }
    return NULL;
}

/* 模拟内核任务B */
void *kernel_task_b(void *args) {
    for (int i = 0; i < 10; ++i) {
        printk("K_B ~~~ %d\n", i);
        HLT
    }
    return NULL;
}

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
