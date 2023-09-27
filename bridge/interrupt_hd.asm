[SECTION .text]
[bits 32]

extern hd_handler

; ---------------------------------------------------------------------------------------
; 硬盘中断调用入口
; ---------------------------------------------------------------------------------------
global interrupt_handler_hd
interrupt_handler_hd:

    call hd_handler

    iret

