//
// Created by Toney on 2023/9/28.
//

#include "../include/lock.h"
#include "../include/eflags.h"
#include "../include/print.h"

extern task_t *current_task;

// 初始化锁
void lock_init(lock_t *l) {
    l->holder = NULL;
    l->repeat = 0;
    l->sema.value = 1; // 初始信号量为1
    chain_init(&l->sema.waiters); // 等待队列
}

/* 获取锁 */
void lock(lock_t *l) {
    // 如果当前任务已经持有锁
    if (l->holder && (l->holder->pid == current_task->pid)) {
        l->repeat++;
        return;
    }

    // 当前任务无锁，申请信号量
    bool iflag = check_close_if(); // 关中断保证原子操作
    while (l->sema.value == 0) {
        // 无可用信号量，将当前任务加到等待队列
        chain_put_last(&l->sema.waiters, &current_task->chain_lock);

        // 阻塞当前任务
        current_task->state = TASK_BLOCKED;  // 将当前任务设为阻塞态
        check_recover_if(iflag);            // 恢复中断
        HLT                                 // 释放当前任务控制权

        // 有可用信号量，当前任务重新被唤醒，进入运行态
        iflag = check_close_if(); // 关中断，重新获取信号量
    }

    // 已有信号量
    l->sema.value--;            // 目前信号量只有0和1，这里将信号量置0
    l->holder = current_task;

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
    bool iflag = check_close_if(); // 关中断保证原子操作

    // 处理等待任务队列
    while (true) {
        // 取出第一个等待的任务
        task_t *task = (task_t *) ((uint32) chain_pop_first(&l->sema.waiters) & 0xFFFFF000);
        if (!task) break; // 如果没有就不管了

        // 这里任务一定是处于阻塞态, 除非任务已经被清理, 那就找下一个
        if (task->state != TASK_BLOCKED) continue;

        // 将其置为就绪态，其下次被调度时就可以申请锁了
        task->state = TASK_READY;
        break; // 唤醒第一个几个，其他的继续等待
    }

    l->sema.value++; // 目前信号量只有0和1，这里将信号量置1
    check_recover_if(iflag); // 恢复中断
}