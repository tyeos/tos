[bits 32]
[SECTION .text]

; -------------------------------------------------------------------------------------------------------------------
; 32位EFLAGS寄存器结构：
;     -------------------------------------------------------------------------------------------------
;     | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  |
;     |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0  | ID  | VIP | VIF | AC  | VM  | RF  |
;     -------------------------------------------------------------------------------------------------
;     -------------------------------------------------------------------------------------------------
;     | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
;     |  0  | NT  |    IOPL   | OF  | DF  |  IF |  TF |  SF |  ZF |  0  |  AF |  0  |  PF |  1  |  CF |
;     -------------------------------------------------------------------------------------------------
; -------------------------------------------------------------------------------------------------------------------
; StatusFlag:
;     OF:   Overflow Flag
;     SF:   Sign Flag
;     ZF:   Zero Flag
;     AF:   Auxiliary Carry Flag
;     PF:   Parity Flag
;     CF:   Carry Flag
; SystemFlag:
;     ID:   ID Flag
;     VIP:  Virtual Interrupt Pending
;     VIF:  Virtual Interrupt Flag
;     AC:   Alignment Check / Access Control
;     VM:   Virtual-8086 Mode
;     RF:   Resume Flag
;     NT:   Nested Task
;     IOPL: I/O Privilege Level
;     IF:   Interrupt Enable Flag
;     TF:   Trap Flag
; ControlFlag:
;     DF:   Direction Flag
; OtherFlag:
;     0/1:  Reserved bit positions. DO NOT USE. Always set to values previously read.
; -------------------------------------------------------------------------------------------------------------------


; -----------------------
; 获取32位标志寄存器
; -----------------------
; eax = get_eflags()
; -----------------------
global get_eflags
get_eflags:
    pushfd
    pop eax
    ret

; -----------------------
; 设置32位标志寄存器
; -----------------------
; set_flags(uint32)
; -----------------------
global set_eflags
set_eflags:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]
    push eax
    popfd
    leave
    ret

; -----------------------
; 获取IF标志位
; -----------------------
; al = get_if_flag()
; -----------------------
global get_if_flag
get_if_flag:
    pushfd
    pop eax
    shr eax, 9
    and eax, 1
    ret
