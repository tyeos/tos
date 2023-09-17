//
// Created by toney on 23-9-16.
//

#include "../include/task.h"
#include "../include/sys.h"
#include "../include/print.h"
#include "../include/chain.h"


extern void switch_task(task_t *); // 在汇编中实现

extern circular_chain_t *tasks;;

task_t *current = NULL;

task_t *next_ready_task() {
    if (tasks == NULL) {
        return NULL;
    }

    task_t * next = NULL;

    for (int i = 0; i < tasks->size; ++i) {
        task_t *task = read_item(tasks)->value;
        if (task->state != TASK_READY) continue;
        next = task;
        break;
    }

    return next;
}

void sched() {
    STI
    if (current != NULL) {
        current->state = TASK_READY;
    }

    current = next_ready_task();
    if (current != NULL) {
        current->state = TASK_RUNNING;
    }

    if (current == NULL) {
        return;
    }

    printk("sched2---\n");
    switch_task(current);
}
