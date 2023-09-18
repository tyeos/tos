//
// Created by toney on 23-9-16.
//

#include "../include/task.h"
#include "../include/sys.h"
#include "../include/print.h"
#include "../include/chain.h"


extern void processing_task(task_t *); // 在汇编中实现

extern circular_chain_t *tasks;;
task_t *current_task = NULL;

task_t *get_next_ready_task() {
    if (tasks == NULL) {
        return NULL;
    }

    task_t *next = NULL;
    for (int i = 0; i < tasks->size; ++i) {
        task_t *task = read_item(tasks)->value;
        if (task->state != TASK_READY) continue;
        next = task;
        break;
    }

    return next;
}

void clock_task_scheduler() {
    // 当前任务置为就绪状态
    if (current_task != NULL) {
        current_task->state = TASK_READY;
    }

    // 换下一个任务
    current_task = get_next_ready_task();
    if (current_task == NULL) {
        return;
    }

    printk("clock_task_scheduler once ===================================== \n");
    current_task->state = TASK_RUNNING;
    processing_task(current_task);
}
