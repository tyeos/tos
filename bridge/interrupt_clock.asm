[SECTION .text]
[bits 32]

extern current_task ; 在c中定义，当前任务
extern clock_task_scheduler ; 在c中定义，任务调度程序

; -------------------------------------------------
; 时钟中断处理函数
; -------------------------------------------------
; 初始栈结构（摘自 interrupt.asm）:
; ---------------------------------------------------------------------------------------
;     eip          ↑ 低地址（ <-esp 当前栈顶）
;     cs           ↑
;     eflags       ↑ （栈的增长方向）
;     (esp)        ↑ 注：从这里开始往下 ↓ 是发生特权级栈切换时额外压入的两个值
;     (ss)         ↑ 高地址
; ---------------------------------------------------------------------------------------
global interrupt_handler_clock
interrupt_handler_clock:

    ; 看当前有没有调度的任务
    push ecx
    mov ecx, [current_task]
    cmp ecx, 0
    je .skip_store_env ; 无需保存上下文环境

.store_env:

    ; -------------------------------------------------------------------
    ; 保存原程序环境，即：初始栈中的寄存器+通用寄存器(pushad)
    ; 保存到 current_task 的 tss 结构中，参考 task.h 的 tss_t 结构定义
    ; -------------------------------------------------------------------
    ; 中断栈结构和pushad结构参考 interrupt.asm 中的中断定义
    ; -------------------------------------------------------------------

    ; 依次保存
    mov [ecx + 17 * 4], edi
    mov [ecx + 16 * 4], esi
    mov [ecx + 15 * 4], ebp
    ; mov [eax + 14 * 4], esp
    mov [ecx + 13 * 4], ebx
    mov [ecx + 12 * 4], edx
    ; mov [ecx + 11 * 4], ecx
    mov [ecx + 10 * 4], eax

    ; eip
    mov eax, [esp + 4]
    mov [ecx + 8 * 4], eax

    ; cs
    mov eax, [esp + 8]
    mov [ecx + 19 * 4], eax

    ; eflags
    mov eax, [esp + 0x0C]
    mov [ecx + 9 * 4], eax

    ; 用eax把ecx换下来
    mov eax, ecx
    pop ecx ; 恢复 ecx 和 esp
    mov [eax + 11 * 4], ecx
    mov [eax + 14 * 4], esp

    ; 恢复eax
    mov eax, [ecx + 10 * 4]

    jmp .start_scheduler

.skip_store_env:
    pop ecx

.start_scheduler: ; 下一步, 开始调度
    call clock_task_scheduler

    iret

