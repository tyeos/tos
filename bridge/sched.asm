[SECTION .text]
[BITS 32]


extern current
extern get_sched_times
extern exit_current_task

; stack struct:
;   return address
;   param
global switch_task
switch_task:
    mov ecx, [esp + 8] ; task

    push ecx
    call get_sched_times
    add esp, 4

    cmp eax, 0
    je .call

.restore_env:
    ; 恢复上下文
    mov ecx, [current]

    ; 以下赋值参考 tss_t 定义
    ; mov eax, [ecx + 10 * 4]
    mov edx, [ecx + 12 * 4]
    mov ebx, [ecx + 13 * 4]
    mov esp, [ecx + 14 * 4]
    mov ebp, [ecx + 15 * 4]
    mov esi, [ecx + 16 * 4]
    mov edi, [ecx + 17 * 4]

    mov eax, ecx
    mov ecx, [eax + 11 * 4]

    mov eax, [eax + 8 * 4]      ; eip
    jmp eax

.call:
    mov ecx, [current]
    mov eax, [ecx + 26*4 + 2*4]
    call eax


.exit:
    call exit_current_task

    ret

