[bits 32]
[SECTION .text]

; 导入C的函数
extern interrupt_handler_callback

; -------------------------------------------------
; 宏定义格式：
; -------------------------------------------------
; %macro 宏名字 参数个数
; 	宏代码体
; %endmacro
;
; 宏名字:
; 	调用宏时使用
;
; 参数个数：
; 	引用某个参数要用“%数字”的方式，数字从1开始
; -------------------------------------------------
; -------------------------------------------------
; 定义（异常）中断处理函数：
; -------------------------------------------------
; 参数:
;   1：idt_index
; -------------------------------------------------
; 在进入中断之前，cpu会向栈中压入三个值：
;   push eflags
;   push cs
;   push eip
; -------------------------------------------------
%macro INTERRUPT_HANDLER 1
global interrupt_handler_%1
interrupt_handler_%1:

    pushad ; 保存现场，把eax、ecx、edx、ebx、esp、ebp、esi、edi依次压入栈中，ESP会减少32

    push %1 ; 压入idt_index, 即中断号
    call interrupt_handler_callback ; 回调给C
    add esp, 4

    popad ; 恢复现场，将栈中数据弹出，依次传给edi、esi、ebp、esp、ebx、edx、ecx、eax，ESP会增加32

    iret
%endmacro


; --------------------------------------------------------------------------------
; 调用宏
; --------------------------------------------------------------------------------
; 中断向量号0~31（0x00~0x1f）有特殊含义，32~255（0x20~0xFF）为可屏蔽中断向量号
; --------------------------------------------------------------------------------
INTERRUPT_HANDLER 0x00; divide by zero
INTERRUPT_HANDLER 0x01; debug
INTERRUPT_HANDLER 0x02; non maskable interrupt
INTERRUPT_HANDLER 0x03; breakpoint

INTERRUPT_HANDLER 0x04; overflow
INTERRUPT_HANDLER 0x05; bound range exceeded
INTERRUPT_HANDLER 0x06; invalid opcode
INTERRUPT_HANDLER 0x07; device not avilable

INTERRUPT_HANDLER 0x08; double fault
INTERRUPT_HANDLER 0x09; coprocessor segment overrun
INTERRUPT_HANDLER 0x0a; invalid TSS
INTERRUPT_HANDLER 0x0b; segment not present

INTERRUPT_HANDLER 0x0c; stack segment fault
INTERRUPT_HANDLER 0x0d; general protection fault
INTERRUPT_HANDLER 0x0e; page fault
INTERRUPT_HANDLER 0x0f; reserved

INTERRUPT_HANDLER 0x10; x87 floating point exception
INTERRUPT_HANDLER 0x11; alignment check
INTERRUPT_HANDLER 0x12; machine check
INTERRUPT_HANDLER 0x13; SIMD Floating - Point Exception

INTERRUPT_HANDLER 0x14; Virtualization Exception
INTERRUPT_HANDLER 0x15; Control Protection Exception

INTERRUPT_HANDLER 0x16; reserved
INTERRUPT_HANDLER 0x17; reserved
INTERRUPT_HANDLER 0x18; reserved
INTERRUPT_HANDLER 0x19; reserved
INTERRUPT_HANDLER 0x1a; reserved
INTERRUPT_HANDLER 0x1b; reserved
INTERRUPT_HANDLER 0x1c; reserved
INTERRUPT_HANDLER 0x1d; reserved
INTERRUPT_HANDLER 0x1e; reserved
INTERRUPT_HANDLER 0x1f; reserved

; --------------------------------------------------------------------------------
; 一般个人计算机中只有两个8259A芯片，即最多支持15个中断，可参考 head.asm 中的定义
; --------------------------------------------------------------------------------
INTERRUPT_HANDLER 0x20; clock 时钟中断
INTERRUPT_HANDLER 0x21; 键盘中断
INTERRUPT_HANDLER 0x22
INTERRUPT_HANDLER 0x23
INTERRUPT_HANDLER 0x24
INTERRUPT_HANDLER 0x25
INTERRUPT_HANDLER 0x26
INTERRUPT_HANDLER 0x27
INTERRUPT_HANDLER 0x28; rtc 实时时钟
INTERRUPT_HANDLER 0x29
INTERRUPT_HANDLER 0x2a
INTERRUPT_HANDLER 0x2b
INTERRUPT_HANDLER 0x2c
INTERRUPT_HANDLER 0x2d
INTERRUPT_HANDLER 0x2e
INTERRUPT_HANDLER 0x2f

; --------------------------------------------------------------------------------
; 导出所有中断处理函数地址
; --------------------------------------------------------------------------------
global interrupt_handler_table
interrupt_handler_table:
    dd interrupt_handler_0x00
    dd interrupt_handler_0x01
    dd interrupt_handler_0x02
    dd interrupt_handler_0x03
    dd interrupt_handler_0x04
    dd interrupt_handler_0x05
    dd interrupt_handler_0x06
    dd interrupt_handler_0x07
    dd interrupt_handler_0x08
    dd interrupt_handler_0x09
    dd interrupt_handler_0x0a
    dd interrupt_handler_0x0b
    dd interrupt_handler_0x0c
    dd interrupt_handler_0x0d
    dd interrupt_handler_0x0e
    dd interrupt_handler_0x0f
    dd interrupt_handler_0x10
    dd interrupt_handler_0x11
    dd interrupt_handler_0x12
    dd interrupt_handler_0x13
    dd interrupt_handler_0x14
    dd interrupt_handler_0x15
    dd interrupt_handler_0x16
    dd interrupt_handler_0x17
    dd interrupt_handler_0x18
    dd interrupt_handler_0x19
    dd interrupt_handler_0x1a
    dd interrupt_handler_0x1b
    dd interrupt_handler_0x1c
    dd interrupt_handler_0x1d
    dd interrupt_handler_0x1e
    dd interrupt_handler_0x1f
    dd interrupt_handler_0x20
    dd interrupt_handler_0x21
    dd interrupt_handler_0x22
    dd interrupt_handler_0x23
    dd interrupt_handler_0x24
    dd interrupt_handler_0x25
    dd interrupt_handler_0x26
    dd interrupt_handler_0x27
    dd interrupt_handler_0x28
    dd interrupt_handler_0x29
    dd interrupt_handler_0x2a
    dd interrupt_handler_0x2b
    dd interrupt_handler_0x2c
    dd interrupt_handler_0x2d
    dd interrupt_handler_0x2e
    dd interrupt_handler_0x2f
