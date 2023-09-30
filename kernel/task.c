//
// Created by toney on 23-9-16.
//

#include "../include/task.h"
#include "../include/print.h"
#include "../include/sys.h"
#include "../include/string.h"
#include "../include/init.h"
#include "../include/ide.h"


/*
 * --------------------------------------------------------------------------------------------------------------------
 * 最终要实现的任务模型：
 * ----------------------------------------------------------------------------
 *      内核进程-内核线程： // task: idle (pid=0)
 *           内核线程A    // task: K_A
 *           内核线程B    // task: K_B
 *      用户进程(单线程)A  // task: U_PA
 *      用户进程(单线程)B  // task: U_PB
 * --------------------------------------------------------------------------------------------------------------------
 * 顺序：
 *      main创建idle任务，之后便退出历史舞台，
 *      然后在idle程序中，启动其他内核任务及用户任务
 * --------------------------------------------------------------------------------------------------------------------
 * 说明：
 *      进程都有独立的页目录表（及页表）
 *      用户进程除了需要分配单独的页目录表之外，本质上和其他内核线程并没有什么区别
 * ----------------------------------------------------------------------------
 * 每个用户进程都有自己的虚拟地址空间，所有用户进程共享内核虚拟地址空间
 *      多进程页目录表示例：
 * ----------------------------------------------------------------------------
 *                                      +------------+
 *                                      |            |
 *                                      |            |
 *                                      |            |
 *                                      |   UserB    |
 *                                      |    3GB     |
 *                                      |            |
 *                                      |            |
 *                                      |            |
 *                                      |            |
 * -------------------------------------+ · · · · · ·+-------------------------------------
 * |                                    ·            ·                                    |
 * |            UserA 3GB               · Kernel 1GB ·            UserC 3GB               |
 * |                                    ·            ·                                    |
 * -------------------------------------+ · · · · · ·+-------------------------------------
 *                                      |            |
 *                                      |            |
 *                                      |            |
 *                                      |   UserD    |
 *                                      |    3GB     |
 *                                      |            |
 *                                      |            |
 *                                      |            |
 *                                      |            |
 *                                      +------------+
 * --------------------------------------------------------------------------------------------------------------------
 */

static bitmap_t pids;
static chain_t tasks;
static chain_elem_pool_t elem_pool;

static task_t *idle_task;
task_t *current_task = NULL;


/*
 * 进行一次任务调度滴答，执行完成current_task即更新
 */
void task_scheduler_ticks() {
    if (tasks.size == 0) return;

    // 检查当前任务
    if (current_task != NULL) {
        // ticks减为0才进行下一个任务调度
        if (current_task->ticks > 0) {
            current_task->ticks--;
            current_task->elapsed_ticks++;
            // printk("clock_task_scheduler ticks\n");
            return;
        }
        // 保存当前任务状态，置为就绪状态（当前任务必须处于运行态）
        if (current_task->state == TASK_RUNNING) current_task->state = TASK_READY;
        current_task->ticks = current_task->priority;
        current_task = NULL;
    }

    // 换下一个任务
    for (int i = 0; i < tasks.size; ++i) {
        task_t *task = (task_t *) chain_pop_first(&tasks)->value;
        if (task->state != TASK_DIED) chain_put_last(&tasks, task->chain_elem); // 正常任务取出后添加到最后
        if (task->state != TASK_READY) continue; // 只查找处于就绪态的任务

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
    chain_remove(&tasks, task->chain_elem);
    chain_pool_ret(&elem_pool, task->chain_elem);
    bitmap_free(&pids, task->pid);
    free_kernel_page(task);
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
        for (int i = 0; i < tasks.size; ++i) {
            clear_task((task_t *) ((uint32) chain_pop_first(&tasks) & 0xFFFFF000));
        }
        return ssp; // 只有idle进程结束, ssp才有意义，即跳回到main
    }

    // 其他任务的终止，换到idle上来调度
    chain_remove(&tasks, idle_task->chain_elem);
    chain_put_last(&tasks, idle_task->chain_elem);
    current_task = idle_task;
    return 0;
}

uint32 get_current_task_pid() {
    if (current_task == NULL) return 0;
    return current_task->pid;
}

static task_t *create_kernel_thread(char *name, uint8 priority, task_func_t func) {
    // 创建任务
    task_t *task = alloc_kernel_page();
    memset(task, 0, PAGE_SIZE);

    // task初始化
    task->pid = bitmap_alloc(&pids);
    task->func = func;
    task->elapsed_ticks = 0;
    task->kstack = (uint32) task + PAGE_SIZE;

    task->state = TASK_READY;
    task->ticks = priority;
    task->priority = priority;
    strcpy(task->name, name);

    // 添加到任务队列末尾
    chain_elem_t *elem = chain_pool_getv(&elem_pool, task);
    task->chain_elem = elem;
    chain_put_last(&tasks, elem);
    return task;
}

static task_t *create_user_process(char *name, uint8 priority, task_func_t func) {
    // 创建任务
    task_t *task = alloc_kernel_page();
    memset(task, 0, PAGE_SIZE);

    // task初始化
    task->pid = bitmap_alloc(&pids);
    task->func = func;
    task->elapsed_ticks = 0;
    task->kstack = (uint32) task + PAGE_SIZE;

    task->state = TASK_READY;
    task->ticks = priority;
    task->priority = priority;
    strcpy(task->name, name);

    // 用户进程有自己的页目录表
    task->pgdir = create_virtual_page_dir();
    // 虚拟地址池初始化
    user_virtual_memory_alloc_init(&task->user_vaddr_alloc);

    // 添加到任务队列末尾
    chain_elem_t *elem = chain_pool_getv(&elem_pool, task);
    task->chain_elem = elem;
    chain_put_last(&tasks, elem);
    return task;
}

static void *idle(void *args) {
    ide_init();

//    create_kernel_thread("K_A", 2, kernel_task_a);
//    create_kernel_thread("K_B", 1, kernel_task_b);
//    create_user_process("U_PA", 1, user_task_a);
//    create_user_process("U_PB", 1, user_task_b);

    bool all_task_end = false;
    for (int i = 0;; ++i) {
        if (!all_task_end && tasks.size == 1) {
            all_task_end = true;
            printk("idle :::::: all task have exited, except for idle ~ %d\n", i);
        }
        SLEEP_ITS(2)
    }
    return NULL;
}

void task_init() {
    pids.map = kmalloc(256 >> 3); // 最多支持分配256个任务
    pids.total = 256;
    bitmap_init(&pids);
    chain_init(&tasks);
    // lock_init(&test_lock);

    // 一页内存最多可分配 PAGE_SIZE/sizeof(chain_elem_t) ≈ 341 个链表元素, 大于任务数即可
    elem_pool.addr = alloc_kernel_page();
    elem_pool.size = PAGE_SIZE;
    chain_pool_init(&elem_pool);

    idle_task = create_kernel_thread("idle", 1, idle); // 第一个创建的任务，pid一定为0
}

