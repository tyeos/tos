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
    TASK_BLOCKED,   // 阻塞，不可运行态，锁竞争时若未取到信号量，等待期间就将其置为阻塞状态，直到其他任务释放的信号量分给该任务后恢复到就绪态
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

    /* 该结构为tss固定写法，不要做调整，在汇编中也会根据变量位置找值 */

} __attribute__((packed)) tss_t; // 目前占用字节数: 27*4=108

/*
 * --------------------------------------------------------------------------------------------------------------------
 * 进程和线程都用这个task结构，进程有独立的页表，所有线程共用自己进程的页表，
 * 该结构也叫PCB, Process Control Block, 进程控制块，
 * 操作系统为每个进程提供一个PCB, 它就是进程的身份证，用它来记录与此进程相关的信息，比如进程状态、PID、优先级等,
 * PCB都属于内核空间，包括用户进程的PCB。
 * --------------------------------------------------------------------------------------------------------------------
 * 一般PCB的基本结构(实际格式取决于操作系统的功能复杂度):
 *      ------------------------------------------
 *      | 寄存器映像（保存进程现场，包括所有寄存器的值）  |
 *      | 栈                                      |
 *      | 栈指针（记录0级栈栈顶的位置）                |
 *      | pid                                     |
 *      | 进程状态（运行/就绪/阻塞）                  |
 *      | 优先级                                   |
 *      | 时间片                                   |
 *      | 页表                                    |
 *      | 打开的文件描述符                          |
 *      | 父进程                                  |
 *      | ....                                   |
 *      ------------------------------------------
 * --------------------------------------------------------------------------------------------------------------------
 * task (PCB) 规划:
 *
 *            kstack -> ------------------- 0xXXX + 4KB
 *                      |      ...        |
 *                      |      ...        |
 *                      |      ...        |
 *                      ------------------- 0xXXX + sizeof(task_t)
 *                      |                 |
 *                      |                 |
 *                      |                 |
 *                      | task attributes |
 *                      | sizeof (task_t) |
 *                      |                 ·----------------- 0xXXX + sizeof(tss_t)
 *                      |                 ·                |
 *                      |                 · tss attributes |
 *                      |                 · sizeof (tss_t) |
 *                      |                 ·                |
 *        low address   ------------------------------------ 0xXXX
 *
 * --------------------------------------------------------------------------------------------------------------------
 */
typedef struct task_t {
    tss_t tss;                // offset=0, 上下文
    uint32 pid;               // offset=sizeof(tss_t)+0*4
    task_func_t func;         // offset=sizeof(tss_t)+1*4, 要执行的任务, 函数指针, 32位模式下占4字节
    uint32 elapsed_ticks;     // offset=sizeof(tss_t)+2*4, 总调度次数（从开始运行的总滴答数）
    uint32 kstack;            // offset=sizeof(tss_t)+3*4, 每个内核任务都有自己的内核栈
    uint32 *pgdir;            // offset=sizeof(tss_t)+4*4 进程页目录表的虚拟地址（该属性给用户进程使用，可用其判断是否是用户任务）

    /* 以上字段位置不要做调整，在汇编中会根据变量位置找值，下面的变量可做调整 */

    task_state_t state;              // 进程状态，enum占4字节
    chain_elem_t *chain_elem;        // 在任务队列中的元素指针
    memory_alloc_t user_vaddr_alloc; // 用户进程的虚拟地址池（该属性给用户进程使用）
    uint8 ticks;                     // 占用CPU的时间滴答数（用中断次数表示，初始值为优先级）
    uint8 priority;                  // 任务优先级，值越大级别越高
    char name[16];                   // 线程名称
} __attribute__((packed)) task_t;

void task_init();

void task_scheduler_ticks();
uint32 exit_current_task();
uint32 get_current_task_pid() ;

#endif //TOS_TASK_H
