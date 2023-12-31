//
// Created by toney on 2023-09-01.
//

#include "../include/dt.h"
#include "../include/print.h"
#include "../include/int.h"
#include "../include/types.h"
#include "../include/sys.h"

/*
 -----------------------------------------------------------------------------------------------------------------
 定义 IDT (Interrupt Vector Table) 描述符属性（中断描述符表中的描述符又称为门，共四种门描述符: 任务门、中断门、陷阱门、调用门）
 注：现代操作系统为了简化开发、提升性能和移植性等原因，很少用到调用门和任务门。Linux的系统调用一般也都是使用中断门。
     中断处理都是在0特权级下进行的，即低特权级可以通过门转移到更高的特权级上。
 -----------------------------------------------------------------------------------------------------------------
 每个门描述符占8字节，其中高32位的第8~15位结构统一如下：
     -------------------------------------------------
     | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |
     |  P  |    DPL    |  S  |         TYPE          |
     -------------------------------------------------
 P:
     Present, 0-不存在于内存中（段异常），1-段存在于内存中
     CPU检测到P值为0时，会转到异常处理程序
     特别注意:
        GDT中的第0个段描述符是不可用的，但IDT却无此限制，第0个门描述符也是可用的，中断向量号为0的中断是除法错。
        但处理器只支持256个中断，即0~254，中断描述符中其余的描述符不可用（这些可以把P位置0）。
 DPL:
     Descriptor Privilege Level, 即门描述符特权级，0~3四种特权级，数字越小特权级越大。
     门的DPL作为访问下限，即只有访问者的(CPL)权限大于等于该DPL表示的最低权限才能够继续访问。
     门描述符中指示的目标代码段的DPL作为访问上限，即只有访问者的(CPL)权限小于等于该DPL表示的最低权限才能够顺利转移，即向高特权级转移。
 S：
    为0时表示系统段，S为1时表示数据段。
    这里的各种门都属于系统段，即S都等于0。
 TYPE:
    和S字段配合在一起才能确定段描述符的确切类型。
    系统段中的TYPE结构（S=0）：
        -----------------
        | 3 | 2 | 1 | 0 |
        ----------------- ----系统段类型---- | ----------说明----------------------------------------------
        | 0 | 0 | 0 | 0 | 未定义            | 保留
        | 0 | 0 | 0 | 1 | 可用的 80286 TSS  | 仅限286的任务状态段
        | 0 | 0 | 1 | 0 | LDT              | 局部描述符表，只有第1位为1
        | 0 | 0 | 1 | 1 | 忙碌的 80286 TSS  | 仅限286。type中的第1位称为B位，若为1，则表示当前任务忙碌。由CPU将此位置1
        | 0 | 1 | 0 | 0 | 80286 调用门      | 仅限286
        | 0 | 1 | 0 | 1 | 任务门            | 任务门在现代操作系统中很少用到
        | 0 | 1 | 1 | 0 | 80286 中断门      | 仅限286
        | 0 | 1 | 1 | 1 | 80286 陷阱门      | 仅限286
        | 1 | 0 | 0 | 0 | 未定义            | 保留
        | 1 | 0 | 0 | 1 | 可用的 80386 TSS  | 386以上CPU的TSS, type第3位为1
        | 1 | 0 | 1 | 0 | 未定义            | 保留
        | 1 | 0 | 1 | 1 | 忙碌的 80386 TSS  | 386以上CPU的TSS, type第3位为1
        | 1 | 1 | 0 | 0 | 80386 调用门      | 386以上CPU的调用门，type第3位为1
        | 1 | 1 | 0 | 1 | 未定义            | 保留
        | 1 | 1 | 1 | 0 | 中断门            | 386以上CPU的中断门
        | 1 | 1 | 1 | 1 | 陷阱门            | 386以上CPU的陷阱门
        ---------------------------------------------------------------------------------------------------

 -----------------------------------------------------------------------------------------------------------------
 任务门：
     任务门和任务状态段(Task Status Segment, TSS)是Intel处理器在硬件一级提供的任务切换机制，
     所以任务门需要和TSS配合在一起使用，在任务门中记录的是TSS选择子，偏移量未使用。
     任务门可以存在于全局描述符表GDT、局部描述符表LDT、中断描述符表IDT中。
     注：大多数操作系统（包括Linux)都未用TSS实现任务切换
 描述符格式：
     高32位：
         -------------------------------------------------------------------------------------------------
         | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  |
         |                                             未使用                                             |
         -------------------------------------------------------------------------------------------------
         -------------------------------------------------------------------------------------------------
         | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
         |  P  |    DPL    |  S  |         TYPE          |                     未使用                     |
         -------------------------------------------------------------------------------------------------
     低32位：
         -------------------------------------------------------------------------------------------------
         | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  |
         |                                           TSS 选择子                                           |
         -------------------------------------------------------------------------------------------------
         -------------------------------------------------------------------------------------------------
         | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
         |                                             未使用                                             |
         -------------------------------------------------------------------------------------------------
     S和TYPE的取值:
         -------------------------------
          ... | S |      TYPE     | ...
         -------------------------------
          ... | 0 | 0 | 1 | 0 | 1 | ...
         -------------------------------

 -----------------------------------------------------------------------------------------------------------------
 中断门:
     中断门包含了中断处理程序所在段的段选择子和段内偏移地址。
     当通过此方式进入中断后，标志寄存器eflags中的F位自动置0，也就是在进入中断后，自动把中断关闭，避免中断嵌套。
     Linux就是利用中断门实现的系统调用，就是那个著名的int 0x80。
     中断门只允许存在于IDT中。
 描述符格式：
     高32位：
         -------------------------------------------------------------------------------------------------
         | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  |
         |                             中断处理程序在目标代码段内的偏移量的16~31位                              |
         -------------------------------------------------------------------------------------------------
         -------------------------------------------------------------------------------------------------
         | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
         |  P  |    DPL    |  S  |         TYPE          |                     未使用                     |
         -------------------------------------------------------------------------------------------------
     低32位：
         -------------------------------------------------------------------------------------------------
         | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  |
         |                                  中断处理程序目标代码段描述符选择子                                  |
         -------------------------------------------------------------------------------------------------
         -------------------------------------------------------------------------------------------------
         | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
         |                               中断处理程序在目标代码段内的偏移量的0~15位                              |
         -------------------------------------------------------------------------------------------------
 S和TYPE的取值:
   -------------------------------
    ... | S |      TYPE     | ...
   -------------------------------
    ... | 0 | D | 1 | 1 | 0 | ...
   -------------------------------
   D: D位为0表示16位模式, D位为1表示32位模式

 -----------------------------------------------------------------------------------------------------------------
 陷阱门：
     陷阱门和中断门非常相似，区别是由陷阱门进入中断后，标志寄存器eflags中的F位不会自动置0。
     陷阱门只允许存在于DT中。
 描述符格式：
     高32位：
         -------------------------------------------------------------------------------------------------
         | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  |
         |                             中断处理程序在目标代码段内的偏移量的16~31位                              |
         -------------------------------------------------------------------------------------------------
         -------------------------------------------------------------------------------------------------
         | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
         |  P  |    DPL    |  S  |         TYPE          |                     未使用                     |
         -------------------------------------------------------------------------------------------------
     低32位：
         -------------------------------------------------------------------------------------------------
         | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  |
         |                                  中断处理程序目标代码段描述符选择子                                  |
         -------------------------------------------------------------------------------------------------
         -------------------------------------------------------------------------------------------------
         | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
         |                               中断处理程序在目标代码段内的偏移量的0~15位                              |
         -------------------------------------------------------------------------------------------------
 S和TYPE的取值:
   -------------------------------
    ... | S |      TYPE     | ...
   -------------------------------
    ... | 0 | D | 1 | 1 | 1 | ...
   -------------------------------
   D: D位为0表示16位模式, D位为1表示32位模式

 -----------------------------------------------------------------------------------------------------------------
 调用门：
     调用门是提供给用户进程进入特权0级的方式，其DPL为3。
     调用门中记录例程的地址，它不能用int指令调用，只能用call和jmp指令。
     调用门可以安装在GDT和LDT中。
 描述符格式：
     高32位：
         -------------------------------------------------------------------------------------------------
         | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  |
         |                             被调用例程在目标代码段内的偏移量的16~31位                                |
         -------------------------------------------------------------------------------------------------
         -------------------------------------------------------------------------------------------------
         | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
         |  P  |    DPL    |  S  |         TYPE          |  0  |  0  |  0  |        参数个数               |
         -------------------------------------------------------------------------------------------------
     低32位：
         -------------------------------------------------------------------------------------------------
         | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  |
         |                                  被调用例程所在代码段的描述符选择子                                  |
         -------------------------------------------------------------------------------------------------
         -------------------------------------------------------------------------------------------------
         | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
         |                               被调用例程在目标代码段内的偏移量的0~15位                               |
         -------------------------------------------------------------------------------------------------
 参数个数：
    这是处理器将用户提供的参数复制给内核时需要用到的，由于特权级的变化，低特权级的程序无法使用高特权级的栈，所以处理器在固件上实现了参数的自动复制，
    参数在栈中的顺序是挨着的，所以处理器只需要知道复制几个参数就行了，它是专门给处理器准备的。该位用5个BIT来表示，即最多可传递31个参数。
 S和TYPE的取值:
   -------------------------------
    ... | S |      TYPE     | ...
   -------------------------------
    ... | 0 | D | 1 | 0 | 0 | ...
   -------------------------------
   D: D位为0表示16位模式, D位为1表示32位模式

 -----------------------------------------------------------------------------------------------------------------
*/
#define IDT_SIZE    0x100 // 256
interrupt_gate idt[IDT_SIZE] = {0};

/*
 -----------------------------------------------------------------------------------------------------------------
 中断描述符表寄存器（Interrupt Descriptor Table Register, IDTR）结构：
     -------------------------------------------------------------------------------
     | 47  |  ...    ...    ...    ...    ...  |  16  |    15  |  ...   ...  |  0  |
     |                  32位的表基址                    |          16位的表界限        |
     -------------------------------------------------------------------------------
     16位的表界限表示最大范围是0xFFFF，即64KB。可容纳的描述符个数是64KB/8=8K=8192个。

 加载IDTR指令：
     lidt 48位内存数据
 -----------------------------------------------------------------------------------------------------------------
*/
dt_ptr idt_ptr;

extern uint interrupt_handler_table[0x30];   // 地址在汇编中定义
extern void interrupt_handler_clock();       // 时钟中断函数，汇编中定义
extern void interrupt_handler_hd_primary();  // 硬盘主通道中断函数，汇编中定义
extern void interrupt_handler_hd_secondary();// 硬盘次通道中断函数，汇编中定义
extern void interrupt_handler_system_call(); // 系统调用函数，汇编中定义

void idt_init() {
    for (int i = 0; i < IDT_SIZE; ++i) {
        interrupt_gate *p = &idt[i];

        uint handler = i < 0x30 ? interrupt_handler_table[i] : 0;
        if (i == 0x20) handler = (uint) interrupt_handler_clock;
        if (i == 0x2E) handler = (uint) interrupt_handler_hd_primary;
        if (i == 0x2F) handler = (uint) interrupt_handler_hd_secondary;
        if (i == 0x80) handler = (uint) interrupt_handler_system_call;

        p->offset0 = handler & 0xffff;
        p->offset1 = (handler >> 16) & 0xffff; // 设置中断处理函数

        p->selector = 1 << 3;           // 代码段 （TI=0,RPL=0,即对应GDT表index=1的段）
        p->reserved = 0;                // 保留不用
        p->type = 0b1110;               // 中断门
        p->segment = 0;                 // 系统段
        p->DPL = (i == 0x80) ? 3 : 0;   // 除允许用户态0x80中断外，其余均为内核态
        p->present = 1;                 // 有效
    }

    // 让CPU知道中断向量表
    idt_ptr.limit = IDT_SIZE * 8;
    idt_ptr.base = (int) idt;
    __asm__ volatile("lidt idt_ptr;");
}

// 从汇编中回调
bool interrupt_handler_callback(int idt_index, int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax,
                                int v0, int v1, int v2, int v3, int v4, int v5) {
    // 由可编程中断控制芯片触发
    if (idt_index >= 0x20 && idt_index < 0x30) {
        // 看cs是r0还是r3, 如果是r0, 则没有esp_r3和ss_r3
        if (!(v1 & 0b11)) v3 = v4 = 0;
        interrupt_handler_pic(idt_index, edi, esi, ebp, esp, ebx, edx, ecx, eax,
                              v0, v1, v2, v3, v4);
        return false;
    }
    // 其他中断，基本都称之为异常 exception (参考 README.md )
    if (interrupt_has_error_code(idt_index)) {
        if (!(v2 & 0b11)) v4 = v5 = 0;
        interrupt_handler_exception(idt_index, edi, esi, ebp, esp, ebx, edx, ecx, eax,
                                    v0, v1, v2, v3, v4, v5);
        return true;
    }
    if (!(v1 & 0b11)) v3 = v4 = 0;
    interrupt_handler_exception(idt_index, edi, esi, ebp, esp, ebx, edx, ecx, eax,
                                0, v0, v1, v2, v3, v4);
    return false;
}