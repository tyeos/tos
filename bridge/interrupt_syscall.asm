[SECTION .data]
GDT_DATA_INDEX equ 2
R0_DATA_SELECTOR equ GDT_DATA_INDEX << 3

R3_DATA_GDT_ENTRY_INDEX equ 6
R3_DATA_SELECTOR equ R3_DATA_GDT_ENTRY_INDEX << 3 | 0b011


[SECTION .text]
[bits 32]

extern syscall_table

; ---------------------------------------------------------------------------------------
; 系统调用入口，给用户使用（ int 0x80 ）
; ---------------------------------------------------------------------------------------
; 暂时约定使用寄存器传值:
; -----------------------------------------------------
;     eax: 调用号
;     ebx: 第1个参数
;     ecx: 第2个参数
;     edx: 第3个参数
; ---------------------------------------------------------------------------------------
global interrupt_handler_system_call
interrupt_handler_system_call:

    ; ---------------------------------------------------------------------------------------
    ; 恢复内核态段寄存器
    ; ---------------------------------------------------------------------------------------
    push eax
    mov eax, R0_DATA_SELECTOR
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    pop eax

    ; ---------------------------------------------------------------------------------------
    ; 当前栈结构
    ; ---------------------------------------------------------------------------------------
    ;     eip          ↑ 低地址（ <-esp 当前栈顶）
    ;     cs           ↑
    ;     eflags       ↑ （栈的增长方向）
    ;     esp          ↑ 注：从这里开始往下 ↓ 是发生态切换时额外压入的两个值
    ;     ss           ↑ 高地址
    ; ---------------------------------------------------------------------------------------

    ; 转发调用
    push edx ; 第3个参数
    push ecx ; 第2个参数
    push ebx ; 第1个参数
    call [syscall_table + eax * 4] ; 通过调用号找到处理函数调用
    add esp, 4 * 3 ; 平栈

    ; ---------------------------------------------------------------------------------------
    ; 给用户态设置段寄存器
    ; ---------------------------------------------------------------------------------------
    push eax
    mov eax, R3_DATA_SELECTOR
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    pop eax

    iret

