[SECTION .text]
[bits 32]

extern current
extern clock_interrupt_handler

; -------------------------------------------------
; 时钟中断处理函数
; -------------------------------------------------
; 在进入中断之前，cpu会向栈中压入三个值：
;   push eflags
;   push cs
;   push eip
; -------------------------------------------------
global interrupt_handler_clock
interrupt_handler_clock:

    push ecx
    mov ecx, [current]
    cmp ecx, 0
    je .pop_ecx

.store_env:
    ; 以下赋值参考 tss_t 定义
    mov [ecx + 10 * 4], eax
    mov [ecx + 12 * 4], edx
    mov [ecx + 13 * 4], ebx
    mov [ecx + 15 * 4], ebp
    mov [ecx + 16 * 4], esi
    mov [ecx + 17 * 4], edi

    ; eip
    mov eax, [esp + 4]
    mov [ecx + 8 * 4], eax

    ; cs
    mov eax, [esp + 8]
    mov [ecx + 19 * 4], eax

    ; eflags
    mov eax, [esp + 0x0C]
    mov [ecx + 9 * 4], eax

    ; ecx
    mov eax, ecx
    pop ecx
    mov [eax + 11 * 4], ecx
    jmp .call_handler

.pop_ecx:
    pop ecx

.call_handler:
    call clock_interrupt_handler

    iret

