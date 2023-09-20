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
    TASK_DIED       // 死亡
} task_state_t;


/*
 * TSS, Task Status Segment
 * TSS为intel为了方便操作系统管理进程而加入的一种结构
 * TSS是一个段，即一块内存，这里保存要切换的进程的cpu信息，包括各种寄存器的值、局部描述表ldt的段选择子等，
 * 切换时cpu会将这段内容存进各自对应的寄存器
 */
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
    uint32 ssp;             // offset=26*4, 任务影子栈指针（目前用于保存main中断到idle任务时的esp）
} __attribute__((packed)) tss_t; // 占用字节数: 27*4=108

/*
 * 以下结构即为PCB，Process Control Block, 即程序控制块，
 * 操作系统为每个进程提供一个PCB, PCB都属于内核空间，包括用户进程的PCB。
 */
typedef struct task_t {
    tss_t tss;                // offset=0, 上下文
    uint32 pid;               // offset=sizeof(tss_t)+0*4
    task_func_t func;         // offset=sizeof(tss_t)+1*4, 要执行的任务, 函数指针, 32位模式下占4字节
    task_state_t state;       // offset=sizeof(tss_t)+2*4, enum占4字节
    uint32 elapsed_ticks;     // offset=sizeof(tss_t)+3*4, 总调度次数（从开始运行的总滴答数）
    chain_elem_t* chain_elem; // offset=sizeof(tss_t)+4*4, 在任务队列中元素指针

    uint32 stack;           // offset=sizeof(tss_t)+5*4, 每个内核任务都有自己的内核栈
    uint8 ticks;            // offset=sizeof(tss_t)+6*4, 占用CPU的时间滴答数（用中断次数表示，初始值为优先级）
    uint8 priority;         // offset=sizeof(tss_t)+6*4+1, 任务优先级，值越大级别越高
    char name[16];          // offset=sizeof(tss_t)+6*4+2, 线程名称
} __attribute__((packed)) task_t;

void task_init();

void task_scheduler_ticks();
uint32 exit_current_task();

#endif //TOS_TASK_H
