[SECTION .text]
[bits 32]

extern hd_interrupt_handler

; ---------------------------------------------------------------------------------------
; 硬盘主通道中断调用入口
; ---------------------------------------------------------------------------------------
global interrupt_handler_hd_primary
interrupt_handler_hd_primary:
    pushad ; 保存当前任务寄存器状态

    push 0x2E
    call hd_interrupt_handler
    add esp, 4

    popad ; 恢复当前任务寄存器状态
    iret

; ---------------------------------------------------------------------------------------
; 硬盘次通道中断调用入口
; ---------------------------------------------------------------------------------------
global interrupt_handler_hd_secondary
interrupt_handler_hd_secondary:
    pushad ; 保存当前任务寄存器状态

    push 0x2F
    call hd_interrupt_handler
    add esp, 4

    popad ; 恢复当前任务寄存器状态
    iret

