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
    TASK_RUNNING,   // 执行，正在运行态，当前运行的线程必然是此状态
    TASK_READY,     // 就绪，可运行态，只有处于此状态的任务才有机会上处理器运行
//    TASK_BLOCKED,   // 阻塞，不可运行态，锁竞争时若未取到信号量，等待期间就将其置为阻塞状态，直到其他任务释放的信号量分给该任务后恢复到就绪态
//    TASK_WAITING,   // 等待，不可运行态，
//    TASK_HANGING,   // 挂起，不可运行态，等CPU通知，CPU从当前任务出去执行新任务，当前任务暂时挂起，新任务执行完成回到此任务才能继续执行
//    TASK_SLEEPING,  // 睡眠，不可运行态，到指定时间再来找CPU
    TASK_DIED       // 死亡，不可运行态，已从或正从任务队列中摘除，不会再恢复执行
} task_state_t;


/*
 -----------------------------------------------------------------------------------------------------------------------
 TSS (Task State Segment) ：
    TSS同其他普通段一样，是位于内存中的区域，因此可以把TSS理解为TSS段，TSS中的数据是按照固定格式来存储的，即TSS是个数据结构。
    CPU有一个专门存储TSS信息的TR寄存器(Task Register)，它始终指向当前正在运行的任务，即对于CPU来说，任务切换的实质就是TR指向不同的TSS。
 -----------------------------------------------------------------------------------------------------------------------
 32位下TSS的结构:
 --------------------------------------------------------------------------
    31                        15                        0
    +-------------------------+-------------------------+
    |  I/O位图在TSS中的偏移地址  |          （保留）      |T| 100
    |        （保留）          |         ldt选择子        | 96
    |        （保留）          |           gs            | 92
    |        （保留）          |           fs            | 88
    |        （保留）          |           ds            | 84
    |        （保留）          |           ss            | 80
    |        （保留）          |           cs            | 76
    |        （保留）          |           es            | 72
    |                       edi                         | 68
    |                       esi                         | 64
    |                       ebp                         | 60
    |                       esp                         | 56
    |                       ebx                         | 52
    |                       edx                         | 48
    |                       ecx                         | 44
    |                       eax                         | 40
    |                     eflags                        | 36
    |                       eip                         | 32
    |                    cr3(pdbr)                      | 28
    |        （保留）          |           SS2           | 24
    |                       esp2                        | 20
    |        （保留）          |           SS1           | 16
    |                       esp1                        | 12
    |        （保留）          |           SS0           | 8
    |                       esp0                        | 4
    |        （保留）          |     上一个任务的TSS指针    | 0
    +-------------------------+-------------------------+
 --------------------------------------------------------------------------
    TSS中有三组栈：SS0和esp0, SS1和esp1, SS2和esp2，CPU在不同特权级下用不同的栈，这三组栈是用来由低特权级往高特权级跳转时用的，
        最低的特权级是3,没有更低的特权级会跳入3特权级，因此，TSS中没有SS3和esp3。
 -----------------------------------------------------------------------------------------------------------------------
 */
typedef struct tss_t {
    uint32 backlink;        // offset=0*4, 前一个任务的链接，保存了前一个任状态段的段选择子
    uint32 esp0;            // offset=1*4, ring0 的栈顶地址
    uint32 ss0;             // offset=2*4, ring0 的栈段选择子
    uint32 esp1;            // offset=3*4, ring1 的栈顶地址
    uint32 ss1;             // offset=4*4, ring1 的栈段选择子
    uint32 esp2;            // offset=5*4, ring2 的栈顶地址
    uint32 ss2;             // offset=6*4, ring2 的栈段选择子
    uint32 cr3;             // offset=7*4
    uint32 eip;             // offset=8*4
    uint32 eflags;          // offset=9*4
    uint32 eax;             // offset=10*4
    uint32 ecx;             // offset=11*4
    uint32 edx;             // offset=12*4
    uint32 ebx;             // offset=13*4
    uint32 esp;             // offset=14*4
    uint32 ebp;             // offset=15*4
    uint32 esi;             // offset=16*4
    uint32 edi;             // offset=17*4
    uint32 es;              // offset=18*4
    uint32 cs;              // offset=19*4
    uint32 ss;              // offset=20*4
    uint32 ds;              // offset=21*4
    uint32 fs;              // offset=22*4
    uint32 gs;              // offset=23*4
    uint32 ldtr;            // offset=24*4, 局部描述符选择子
    uint16 trace: 1;        // offset=25*4, 如果置位，任务切换时将引发一个调试异常
    uint16 reversed: 15;    // offset=25*4+1, 保留不用
    uint16 iobase;          // offset=25*4+16, I/O 位图基地址，16 位从 TSS 到 IO 权限位图的偏移

    uint32 ssp;             // offset=26*4, 任务影子栈指针（目前用于保存main中断到idle任务时的esp）
} __attribute__((packed)) tss_t; // 目前占用字节数: 27*4=108

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
uint32 get_current_task_pid() ;

#endif //TOS_TASK_H
