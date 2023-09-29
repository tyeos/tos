[SECTION .text]
[bits 32]

extern hd_interrupt_handler

; ---------------------------------------------------------------------------------------
; 硬盘中断调用入口
; ---------------------------------------------------------------------------------------
global interrupt_handler_hd
interrupt_handler_hd:

    ; 保存当前任务寄存器状态
    pushad

    push 0x2E
    call hd_interrupt_handler
    add esp, 4

    ; 恢复当前任务寄存器状态
    popad

    iret

