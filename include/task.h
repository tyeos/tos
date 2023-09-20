//
// Created by toney on 23-9-16.
//

#ifndef TOS_TASK_H
#define TOS_TASK_H

#include "mm.h"
#include "chain.h"

typedef void *(*task_func_t)(void *);

/*进程或线程的状态*/
typedef enum task_state_t {
//    TASK_INIT,      // 初始化
    TASK_RUNNING,   // 执行
    TASK_READY,     // 就绪
//    TASK_BLOCKED,   // 阻塞
//    TASK_WAITING,   // 等待
//    TASK_SLEEPING,  // 睡眠 (到点儿再来找我)
//    TASK_HANGING,   // 挂起 (等我通知再过来)
//    TASK_DIED       // 死亡
} task_state_t;


// Task Status Segment
typedef struct tss_t {
    uint32 backlink; // offset=0*4, 前一个任务的链接，保存了前一个任状态段的段选择子
    uint32 esp0;     // offset=1*4, ring0 的栈顶地址
    uint32 ss0;      // offset=2*4, ring0 的栈段选择子
    uint32 esp1;     // offset=3*4, ring1 的栈顶地址
    uint32 ss1;      // offset=4*4, ring1 的栈段选择子
    uint32 esp2;     // offset=5*4, ring2 的栈顶地址
    uint32 ss2;      // offset=6*4, ring2 的栈段选择子
    uint32 cr3;      // offset=7*4
    uint32 eip;      // offset=8*4
    uint32 flags;    // offset=9*4
    uint32 eax;      // offset=10*4
    uint32 ecx;      // offset=11*4
    uint32 edx;      // offset=12*4
    uint32 ebx;      // offset=13*4
    uint32 esp;      // offset=14*4
    uint32 ebp;      // offset=15*4
    uint32 esi;      // offset=16*4
    uint32 edi;      // offset=17*4
    uint32 es;       // offset=18*4
    uint32 cs;       // offset=19*4
    uint32 ss;       // offset=20*4
    uint32 ds;       // offset=21*4
    uint32 fs;       // offset=22*4
    uint32 gs;       // offset=23*4

    uint32 ldtr;            // offset=24*4, 局部描述符选择子
    uint16 trace: 1;        // offset=25*4, 如果置位，任务切换时将引发一个调试异常
    uint16 reversed: 15;    // offset=25*4+1, 保留不用
    uint16 iobase;          // offset=25*4+16, I/O 位图基地址，16 位从 TSS 到 IO 权限位图的偏移
    uint32 ssp;             // offset=26*4, 任务影子栈指针
} __attribute__((packed)) tss_t; // 占用字节数: 27*4=108


typedef struct task_t {
    tss_t tss;                // offset=0, 上下文
    int pid;                  // offset=sizeof(tss)+0*4
    task_func_t func;         // offset=sizeof(tss)+1*4, 要执行的任务, 函数指针, 32位模式下占4字节
    task_state_t state;       // offset=sizeof(tss)+2*4, enum占4字节
    uint sched_times;         // offset=sizeof(tss)+3*4, 总调度次数
    chain_elem_t* chain_elem; // offset=sizeof(tss)+4*4, 在任务队列中元素指针
} task_t;

//typedef union task_union_t {
//    task_t task;
//    char stack[PAGE_SIZE];
//} task_union_t;

task_t *create_task(task_func_t func);

void task_init();

#endif //TOS_TASK_H
