//
// Created by toney on 23-9-16.
//

#ifndef TOS_TASK_H
#define TOS_TASK_H

#include "mm.h"

typedef void *(*task_func_t)(void *);

/*进程或线程的状态*/
typedef enum task_state_t {
//    TASK_INIT,      // 初始化
    TASK_RUNNING,   // 执行
    TASK_READY,     // 就绪
//    TASK_BLOCKED,   // 阻塞
//    TASK_WAITING,   // 等待
//    TASK_SLEEPING,  // 睡眠 (到点儿再来找我)
    TASK_HANGING,   // 挂起 (等我通知再过来)
    TASK_DIED       // 死亡
} task_state_t;


// Task Status Segment
typedef struct tss_t {
    uint32 backlink; // index=0, 前一个任务的链接，保存了前一个任状态段的段选择子
    uint32 esp0;     // index=1, ring0 的栈顶地址
    uint32 ss0;      // index=2, ring0 的栈段选择子
    uint32 esp1;     // index=3, ring1 的栈顶地址
    uint32 ss1;      // index=4, ring1 的栈段选择子
    uint32 esp2;     // index=5, ring2 的栈顶地址
    uint32 ss2;      // index=6, ring2 的栈段选择子
    uint32 cr3;      // index=7
    uint32 eip;      // index=8
    uint32 flags;    // index=9
    uint32 eax;      // index=10
    uint32 ecx;      // index=11
    uint32 edx;      // index=12
    uint32 ebx;      // index=13
    uint32 esp;      // index=14
    uint32 ebp;      // index=15
    uint32 esi;      // index=16
    uint32 edi;      // index=17
    uint32 es;       // index=18
    uint32 cs;       // index=19
    uint32 ss;       // index=20
    uint32 ds;       // index=21
    uint32 fs;       // index=22
    uint32 gs;       // index=23

    uint32 ldtr;            // index=24, 局部描述符选择子
    uint16 trace: 1;        // index=25, 如果置位，任务切换时将引发一个调试异常
    uint16 reversed: 15;    // index=25, 保留不用
    uint16 iobase;          // index=25(高2字节), I/O 位图基地址，16 位从 TSS 到 IO 权限位图的偏移
    uint32 ssp;             // index=26, 任务影子栈指针
} __attribute__((packed)) tss_t; // 固定 27*4=108 字节


typedef struct task_t {
    tss_t tss;          // 上下文
    int pid;            // index=27
    task_func_t func;   // index=28, 要执行的任务, 函数指针, 32位模式下占4字节
    task_state_t state; // index=29, enum占4字节
    uint sched_times;   // index=30, 总调度次数
} task_t;

//typedef union task_union_t {
//    task_t task;
//    char stack[PAGE_SIZE];
//} task_union_t;

task_t *create_task(task_func_t func);

void task_init();

#endif //TOS_TASK_H
