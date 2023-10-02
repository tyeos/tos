//
// Created by toney on 9/30/23.
//

#include "../../include/task.h"
#include "../../include/print.h"
#include "../../include/sys.h"

/*
 * idle为空闲任务，只管初始创建一些任务，之后空转，其他的事情不管
 */
_Noreturn void *idle(void *args) {
    create_kernel_thread("K_IDE", 2, kernel_task_ide);
//    create_kernel_thread("K_A", 2, kernel_task_a);
//    create_kernel_thread("K_B", 1, kernel_task_b);
//    create_user_process("U_PA", 1, user_task_a);
//    create_user_process("U_PB", 1, user_task_b);

    while (true) {
        if (get_running_task_no() == 1) printk("idle :::::: all task have exited, except for idle ~\n");
        SLEEP(300)
    }
}