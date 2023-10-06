//
// Created by toney on 9/30/23.
//

#include "../../include/task.h"
#include "../../include/print.h"
#include "../../include/sys.h"
#include "../../include/lock.h"
#include "../../include/ide.h"
#include "../../include/string.h"
#include "../../include/shell.h"


/* 模拟内核任务A */
void *kernel_task_a(void *args) {
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

_Noreturn void *kernel_task_shell(void *args) {
    while (true) {
        check_exec_shell();
        HLT // 执行完一次即释放其控制权
    }
}

/*
 * 处理硬盘初始化
 */
void *kernel_task_ide(void *args) {
    printk("K_IDE start~\n");
    ide_init();

    printk("K_IDE end~\n");
    SLEEP(100)
    printk("K_IDE exit~\n");
    return NULL;
}

