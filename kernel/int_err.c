//
// Created by toney on 2023-09-23.
//


#include "../include/sys.h"
#include "../include/types.h"
#include "../include/print.h"
#include "../include/int.h"

/*
 ---------------------------------------------------
 CPU 预设的异常列表（README.md 中有英文原版）：
 ----------------------------------------------------------------------------------------------------------------------------------------------------------------------
 | 向量号  | 助记符       | 类型     | 来源                                      | 描述                                                                                   |
 ----------------------------------------------------------------------------------------------------------------------------------------------------------------------
 | 0      | #DE         | 错误     | DVI和IDIV指令                             | 除零错误                                                                                |
 | 1      | #DB         | 错误/陷阱 | 任何代码或数据引用                          | 调试异常，用于软件调试                                                                     |
 | 2      | /           | 中断     | 不可屏蔽的外部中断                          | NMI中断                                                                                |
 | 3      | #BP         | 陷阱     | INT 3指令                                 | 断点                                                                                   |
 | 4      | #OF         | 陷阱     | INTO指令                                  | 溢出                                                                                   |
 | 5      | #BR         | 错误     | BOUND指令                                 | 数组越界                                                                               |
 | 6      | #UD         | 错误     | UD2指令（奔腾Pro CPU引入此指令）或任何保留的指令 | 无效指令（没有定义的指令）                                                                 |
 | 7      | #NM         | 错误     | 浮点或WAIT/FWAIT指令                       | 数学协处理器不存在或不可用                                                                  |
 | 8      | #DF         | 终止     | 任何可能产生异常的指令、不可屏蔽中断或可屏蔽中断  | 双重错误（Double Fault）                                                                 |
 | 9      | #MF         | 错误     | 浮点指令                                  | 向协处理器传送操作数时检测到页错误（Page Fault）或段不存在，486及以后集成了协处理器，本错误就保留不用了 |
 | 10     | #TS         | 错误     | 任务切换或访问TSS                           | 无效TSS                                                                                |
 | 11     | #NP         | 错误     | 加载段寄存器或访问系统段                      | 段不存在                                                                                |
 | 12     | #SS         | 错误     | 栈操作或加载SS寄存器                         | 栈段错误                                                                                |
 | 13     | #GP         | 错误     | 任何内存引用或保护性检查                      | 通用/一般保护异常，如果一个操作违反了保护模式下的规定，而且该情况不属于其他异常，CPU就是认为是该异常    |
 | 14     | #PF         | 错误     | 任何内存引用                                | 页错误                                                                                 |
 | 15     | /           | 保留     | &nbsp;                                   | /                                                                                     |
 | 16     | #MF         | 错误     | 浮点或WAIT/FWAIT指令                       | 浮点错误                                                                                |
 | 17     | #AC         | 错误     | 对内存中数据的引用（486CPU引入）              | 对齐检查                                                                                |
 | 18     | #MC         | 终止     | 错误代码和来源与型号有关（奔腾CPU引入）          | 机器检查（Machine Check）                                                              |
 | 19     | #XF         | 错误     | SIMD浮点指令（奔腾III CPU引入）               | SIMD浮点异常                                                                          |
 | 20~31  | /           | 保留     | /                                         | /                                                                                    |
 | 32~255 | 用户自定义中断 | 中断     | 来自INTR的外部中断或INT n指令                | 可屏蔽中断                                                                              |
 ----------------------------------------------------------------------------------------------------------------------------------------------------------------------
 */

char *messages[] = {
        "#DE Divide Error\0",
        "#DB RESERVED\0",
        "--  NMI Interrupt\0",
        "#BP Breakpoint\0",
        "#OF Overflow\0",
        "#BR BOUND Range Exceeded\0",
        "#UD Invalid Opcode (Undefined Opcode)\0",
        "#NM Device Not Available (No Math Coprocessor)\0",
        "#DF Double Fault\0",
        "#MF Coprocessor Segment Overrun (reserved)\0",
        "#TS Invalid TSS\0",
        "#NP Segment Not Present\0",
        "#SS Stack-Segment Fault\0",
        "#GP General Protection\0",
        "#PF Page Fault\0",
        "--  (Intel reserved. Do not use.)\0",
        "#MF x87 FPU Floating-Point Error (Math Fault)\0",
        "#AC Alignment Check\0",
        "#MC Machine Check\0",
        "#XF SIMD Floating-Point Exception\0",
        "#VE Virtualization Exception\0",
        "#CP Control Protection Exception\0",
};


// 异常处理
void interrupt_handler_exception(int vector_no, int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax, int eip, char cs, int eflags) {
    printk("\n============================\n");
    printk("EXCEPTION : %s \n", messages[vector_no]);
    printk("   VECTOR : 0x%02X\n", vector_no);
    printk("   EFLAGS : 0x%08X\n", eflags);
    printk("       CS : 0x%02X\n", cs);
    printk("      EIP : 0x%08X\n", eip);
    printk("      ESP : 0x%08X\n", esp);
    printk("============================\n");

    while (true) {
        printk("ERROR STOP!\n");
        CLI
        HLT
    }
}