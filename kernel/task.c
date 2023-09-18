//
// Created by toney on 23-9-16.
//

#include "../include/task.h"
#include "../include/print.h"
#include "../include/sys.h"
#include "../include/chain.h"
#include "../include/bitmap.h"
#include "../include/string.h"


bitmap_t *pids;

circular_chain_t *tasks;
extern task_t *current_task;


// 任务执行完成，退出任务
void exit_current_task() {
    remove_item_by_value(tasks, current_task);
    free_bit(pids, current_task->pid);
    free_page(current_task);
    printk("exit task %p\n", current_task);
    current_task = NULL;
}

task_t *create_task(task_func_t func) {
    // 创建任务
    task_t *task = alloc_page();
    memset(task, 0, PAGE_SIZE);

    task->pid = alloc_bit(pids);
    task->func = func;
    task->sched_times = 0;
    task->state = TASK_READY;

    // 添加到任务队列
    chain_t *item = malloc(sizeof(chain_t));
    item->value = task;
    put_last(tasks, item);

    return task;
}

void *task_func_test1(void *args) {
    for (int i = 0; i < 10000; ++i) {
        printk("A %d \n", i);
    }
    return 0;
}

void *task_func_test2(void *args) {
    for (int i = 0; i < 10000; ++i) {
        printk("B %d ", i++);
    }
    return 0;
}

void task_init() {
    pids = malloc(sizeof(bitmap_t));
    pids->map = malloc(1024 >> 3); // 最多支持分配1024个任务
    pids->cursor = 0;
    pids->used = 0;
    pids->total_bits = 1024;

    tasks = malloc(sizeof(circular_chain_t));
    tasks->size = 0;
    tasks->current = NULL;

    create_task(task_func_test1);
//    create_task(task_func_test2);
}


