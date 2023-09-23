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

task_t *current_task = NULL;
task_t *idle_task;


/*
 * 进行一次任务调度滴答，执行完成current_task即更新
 */
void task_scheduler_ticks() {
    if (tasks == NULL) return;

    // 检查当前任务
    if (current_task != NULL) {
        // ticks减为0才进行下一个任务调度
        if (current_task->ticks > 0) {
            current_task->ticks--;
            current_task->elapsed_ticks++;
            // printk("clock_task_scheduler ticks\n");
            return;
        }
        // 保存当前任务状态，置为就绪状态
        current_task->state = TASK_READY;
        current_task->ticks = current_task->priority;
    }

    // 换下一个任务
    for (int i = 0; i < tasks->size; ++i) {
        task_t *task = chain_pop_first(tasks)->value;
        chain_put_last(tasks, task->chain_elem); // 取出后添加到最后
        if (task->state != TASK_READY) continue;

        task->ticks--;
        task->elapsed_ticks++;
        task->state = TASK_RUNNING;
        current_task = task;
        // printk("clock_task_scheduler once\n");
        return;
    }
}

static void clear_task(task_t *task) {
    printk("ready clear task %p\n", task);
    task->state = TASK_DIED;
    chain_remove(tasks, task->chain_elem);
    mfree(task->chain_elem, sizeof(chain_elem_t));
    free_bit(pids, task->pid);
    free_page(task);
}

// 任务执行完成，退出任务
uint32 exit_current_task() {
    if (current_task == NULL) return 0;
    bool is_idle = current_task == idle_task;
    uint32 ssp = current_task->tss.ssp;
    clear_task(current_task);
    current_task = NULL;

    // 若idle任务终止，则清理所有任务
    if (is_idle) {
        idle_task = NULL;
        for (int i = 0; i < tasks->size; ++i) {
            clear_task(chain_pop_first(tasks)->value);
        }
        return ssp; // 只有idle进程结束, ssp才有意义，即跳回到main
    }

    // 其他任务的终止，换到idle上来调度
    chain_remove(tasks, idle_task->chain_elem);
    chain_put_last(tasks, idle_task->chain_elem);
    current_task = idle_task;
    return 0;
}

uint32 get_current_task_pid() {
    if (current_task == NULL) return 0;
    return current_task->pid;
}

static task_t *create_task(char *name, uint8 priority, task_func_t func) {
    // 创建任务
    task_t *task = alloc_page();
    memset(task, 0, PAGE_SIZE);

    // 任务队列item
    chain_elem_t *elem = malloc(sizeof(chain_elem_t));

    // task初始化
    task->pid = alloc_bit(pids);
    task->func = func;
    task->state = TASK_READY;
    task->elapsed_ticks = 0;
    task->chain_elem = elem;

    task->stack = (uint32) task + PAGE_SIZE;
    task->ticks = priority;
    task->priority = priority;
    strcpy(task->name, name);

    // 添加到任务队列末尾
    elem->value = task;
    chain_put_last(tasks, elem);
    return task;
}

static void *task_test1(void *args) {
    for (int i = 0; i < 20; ++i) {
        printk("A======= %d\n", i);
        HLT
    }
    return NULL;
}

static void *task_test2(void *args) {
    for (int i = 0; i < 50; ++i) {
        printk("B==================================================== %d\n", i);
        HLT
    }
    return NULL;
}

// 测试切换到用户态, 在汇编中实现
extern void *move_to_user_mode(void *args);

static void *idle(void *args) {
//    create_task("task A", 2, task_test1);
    create_task("task B", 2, task_test2);

    create_task("init", 1, move_to_user_mode);

    for (int i = 0;; ++i) {
        printk("idle================================ %d\n", i);
        HLT
        HLT
    }
    return NULL;
}

void task_init() {
    pids = malloc(sizeof(bitmap_t));
    pids->map = malloc(1024 >> 3); // 最多支持分配1024个任务
    pids->cursor = 0;
    pids->used = 0;
    pids->total_bits = 1024;

    tasks = malloc(sizeof(chain_t));
    tasks->head = malloc(sizeof(chain_elem_t));
    tasks->tail = malloc(sizeof(chain_elem_t));
    chain_init(tasks);

    idle_task = create_task("idle", 1, idle); // 第一个创建的任务，pid一定为0
}

