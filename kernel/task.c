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

chain_t *tasks;
extern task_t *current_task;


// 任务执行完成，退出任务
void exit_current_task() {
    printk("ready exit task %p\n", current_task);
    // current_task->state = TASK_DIED;
    chain_remove(tasks,current_task->chain_elem);
    free_bit(pids, current_task->pid);
    free_page(current_task);
    current_task = NULL;
}

task_t *create_task(task_func_t func) {
    // 创建任务
    task_t *task = alloc_page();
    memset(task, 0, PAGE_SIZE);

    // 任务队列item
    chain_elem_t *elem = malloc(sizeof(chain_elem_t));

    // task初始化
    task->pid = alloc_bit(pids);
    task->func = func;
    task->state = TASK_READY;
    task->sched_times = 0;
    task->chain_elem = elem;

    // 添加到任务队列
    elem->value = task;
    chain_put_last(tasks, elem);

    return task;
}

void *task_func_test1(void *args) {
    for (int i = 0; i < 100; ++i) {
        printk("A======= %d\n", i);
    }
    return 0;
}

void *task_func_test2(void *args) {
    for (int i = 0; i < 10000; ++i) {
        printk("B=============== %d\n", i);
    }
    return 0;
}

void *idle_task(void *args) {
//    create_task(task_func_test1);
//    create_task(task_func_test2);

    for (int i = 0; ; ++i) {
        printk("idle %d\n", i);
//        HLT
    }
    return 0;
}

void task_init() {
    pids = malloc(sizeof(bitmap_t));
    pids->map = malloc(1024 >> 3); // 最多支持分配1024个任务
    pids->cursor = 0;
    pids->used = 0;
    pids->total_bits = 1024;

    tasks = malloc(sizeof(chain_t));
    tasks->head = malloc(sizeof(chain_elem_t ));
    tasks->tail = malloc(sizeof(chain_elem_t ));
    chain_init(tasks);

    create_task(idle_task); // 第一个创建的任务，pid一定为0
}


