//
// Created by Toney on 2023/9/28.
//

#include "../include/lock.h"
#include "../include/eflags.h"
#include "../include/print.h"

static chain_elem_pool_t waiter_pool;
extern task_t *current_task;

void semaphore_init() {
    // 支持重复调用，初始化一次即可
    if (waiter_pool.addr) return;
    // 信号量等待队列池，一般信号量都是频繁使用释放，但不会用同时很多，这里申请一页内存给所有使用信号量的地方共享
    waiter_pool.addr = alloc_kernel_page();
    waiter_pool.size = PAGE_SIZE;
    chain_pool_init(&waiter_pool);
}

// 初始化信号量（信号量支持单独使用，也可以配合锁一起用）
void sema_init(semaphore_t* sema, uint8 init_value) {
    sema->value = init_value;             // 初始信号量
    chain_init(&sema->waiters);     // 初始化等待队列
    semaphore_init();                     // 初始化等待队列池
}

// 初始化锁
void lock_init(lock_t *l) {
    l->holder = NULL;
    l->repeat = 0;
    sema_init(&l->sema, 1); // 初始信号量为1
}

// 申请信号量
void sema_down(semaphore_t* sema) {
    // 关中断保证原子操作
    bool iflag = check_close_if();
    while (!sema->value) {
        // 无可用信号量，将当前任务加到等待队列
        chain_put_last(&sema->waiters, chain_pool_getv(&waiter_pool, current_task));

        // 阻塞当前任务
        current_task->state = TASK_BLOCKED; // 将当前任务设为阻塞态
        check_recover_if(iflag);            // 恢复中断
        HLT                                 // 释放当前任务控制权

        // 信号量被释放，当前任务重新被唤醒，进入运行态
        iflag = check_close_if();           // 关中断，重新获取信号量
    }

    // 已有信号量，进行分配
    sema->value--;            // 目前信号量只有0和1，这里将信号量置0
    check_recover_if(iflag);  // 恢复中断
}

/* 获取锁 */
void lock(lock_t *l) {
    if (l->holder && (l->holder->pid == current_task->pid)) {
        // 当前任务已经持有锁
        l->repeat++;
        return;
    }

    // 当前任务无锁，申请信号量
    sema_down(&l->sema);

    // 已申请到
    l->holder = current_task;
}

// 释放/恢复 信号量
void sema_up(semaphore_t* sema) {
    bool iflag = check_close_if(); // 关中断保证原子操作
    // 预处理等待任务队列
    while (true) {
        chain_elem_t *elem = chain_pop_first(&sema->waiters); // 取出第一个等待的任务
        if (!elem) break; // 已经没有等待的任务
        task_t *task = (task_t *) elem->value;
        chain_pool_ret(&waiter_pool, elem);
        // 一般任务一定是处于阻塞态, 除非任务已经被清理, 那就找下一个
        if (task->state != TASK_BLOCKED) continue;
        // 将其置为就绪态，其下次被调度时就可以申请锁了
        task->state = TASK_READY;
        // 如果只唤醒第一个等待的任务，有可能其还没等到进入运行态就被回收掉了，那其他任务就全阻塞了
        // 所以这里将所有等待任务都唤醒，让其自行竞争，不在这里break
    }
    sema->value++; // 目前信号量只有0和1，这里将信号量置1
    check_recover_if(iflag); // 恢复中断
}

/* 释放锁 */
void unlock(lock_t *l) {
    if (l->holder == NULL || (l->holder->pid != current_task->pid)) {
        // 持有锁和释放锁的不是同一个任务，则停止
        printk("unlock error: %d not %d\n", l->holder->pid, current_task->pid);
        STOP
    }
    // 如果当前任务已经持有锁
    if (l->repeat) {
        l->repeat--;
        return;
    }

    l->holder = NULL;   // 持有者先释放
    l->repeat = 0;

    // 恢复信号量
    sema_up(&l->sema);
}