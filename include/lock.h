//
// Created by Toney on 2023/9/28.
//

#ifndef TOS_LOCK_H
#define TOS_LOCK_H

#include "types.h"
#include "chain.h"
#include "task.h"

/* 信号量 */
typedef struct semaphore_t {
    uint8 value;        // 信号量值，即剩余可分配资源的个数
    chain_t waiters;    // 等待队列
} semaphore_t;


/* 基于信号量的锁 */
typedef struct lock_t {
    task_t *holder;    // 锁的持有者（只有多任务可能用到锁）
    semaphore_t sema;  // 用二元信号量实现锁
    uint32 repeat;     // 锁的持有者重复申请锁的次数
} lock_t;

void sema_init(semaphore_t* sema, uint8 init_value);
void sema_down(semaphore_t* sema);
void sema_up(semaphore_t* sema);

void lock_init(lock_t *l);
void lock(lock_t *l);
void unlock(lock_t *l);

#endif //TOS_LOCK_H
