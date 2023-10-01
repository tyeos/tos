//
// Created by toney on 9/30/23.
//

#include "../../include/task.h"
#include "../../include/print.h"
#include "../../include/sys.h"
#include "../../include/ide.h"

void *idle(void *args) {
    ide_init();

    create_kernel_thread("K_A", 2, kernel_task_a);
    create_kernel_thread("K_B", 1, kernel_task_b);
    create_user_process("U_PA", 1, user_task_a);
//    create_user_process("U_PB", 1, user_task_b);

    bool all_task_end = false;
    for (int i = 0;; ++i) {
        if (!all_task_end && get_running_task_no() == 1) {
            all_task_end = true;
            printk("idle :::::: all task have exited, except for idle ~ %d\n", i);
        }
        SLEEP_ITS(2)
    }
    return NULL;
}