[SECTION .data]
msg_error_recover_end:
    db "error recover end!", 10, 13, 0

msg_start_next_scheduler:
    db "start next scheduler!", 10, 13, 0

JUMP_TASK_CODE_ADDR equ 0xE20 ; 临时跳转地址
TSS_STRUCT_SIZE equ 27 << 2 ; tss_t 占用字节数

[SECTION .text]
[BITS 32]

extern exit_current_task ; 在c中定义，退出任务
extern current_task ; 在c中定义，当前任务
extern clock_task_scheduler ; 在c中定义，任务调度程序

extern printk

; ---------------------------------
; 开始处理调度的任务
; ---------------------------------
; void processing_task(task_t *)
; ---------------------------------
global processing_task
processing_task:

    ; 先保存要用的寄存器
    push eax
    push ecx

    ; ---------------------------------------------------------------------------------------
    ; 当前栈结构（C程序调用进来）
    ; ---------------------------------------------------------------------------------------
    ;     ecx          ↑ 低地址（ <-esp 当前栈顶）
    ;     eax          ↑
    ;     return addr  ↑
    ;     task_t *     ↑ 高地址
    ; ---------------------------------------------------------------------------------------

    mov eax, [esp + 0x0C] ; task

    ; 检查是否首次调度先
    mov ecx, [eax + 30 * 4] ; 总调度次数
    inc ecx
    mov [eax + 30 * 4], ecx ; 先把调度次数加了
    cmp ecx, 1
    je .run_new_task ; 首次调度无需恢复上下文环境 （暂不考虑调度次数超过0xFFFFFFFF后归0的情况, 10ms一次归零也需要497天）

.recover_env:

    ; -------------------------------------------------------------------
    ; 恢复任务上下文, 参考 interrupt_clock.asm 中的保存上下文。
    ; -------------------------------------------------------------------
    ; 这里的主要任务就是恢复到之前的任务，并且不再回来，
    ; 即栈要恢复到创建任务时的栈，代码要跳转到之前中断时的代码。
    ; -------------------------------------------------------------------

    mov edi, [eax + 17 * 4]
    mov esi, [eax + 16 * 4]
    mov ebp, [eax + 15 * 4]
    mov esp, [eax + 14 * 4] ; 当前esp不再使用，这里将esp退回到原栈，即任务执行结束后，栈会平到新任务执行结束处，而非回到这里的栈
    mov ebx, [eax + 13 * 4]
    mov edx, [eax + 12 * 4]
    ; mov ecx, [eax + 11 * 4]
    ; mov eax, [eax + 10 * 4]

.recover_run:

    ; eflags（这里临时保存到栈，最后跳转前再恢复）
    mov ecx, [eax + 9 * 4]
    push ecx

    ; eip（cs暂不考虑，eip通过jmp指令恢复)
    mov ecx, [eax + 8 * 4]
    mov [JUMP_TASK_CODE_ADDR], ecx

    ; 完全恢复之前的环境
    mov ecx, [eax + 11 * 4]
    mov eax, [eax + 10 * 4]

    ; eflags（从栈中恢复）
    popfd

    ; -------------------------------------------------------------------------
    ; eflags恢复后就可以发生中断了
    ; -------------------------------------------------------------------------
    ; 如果从这里发生中断，那么中断时保存任务上下文中的eip将会变成当前这行代码的eip，
    ;     在此之后，如果是运行新任务不会影响该地址下的值，即仍可正常跳转运行，
    ;     但如果是中断后又走了恢复旧任务的逻辑，将该地址处的值覆盖，则会产生错误。
    ; 但这只是理论上，因为cpu中断后，做的第一件事就是尽快执行到这里，进行任务调度，
    ;     如果调度流程都还执行不完，就更别提任务的执行了，
    ;     CPU的指令都是纳秒级，执行到此处完全是绰绰有余的，这里就不考虑了。
    ; -------------------------------------------------------------------------
    jmp [JUMP_TASK_CODE_ADDR]

.error_recover_end:
    ; 由于上面已经平到之前的栈，这里不会执行到
    push msg_error_recover_end
    call printk
    cli
    sti

.run_new_task:
    ; 新任务执行
    mov eax, [eax + TSS_STRUCT_SIZE + 4] ; func
    sti ; 允许中断
    call eax

.check_end_task:
    ; 任务执行完成，如果是0号任务则正常返回，非0号任务的结束则继续下一个任务调度
    mov eax, [current_task]
    mov eax, [eax + TSS_STRUCT_SIZE] ; pid
    cmp eax, 0
    jne .next_scheduler

.exit: ; 结束当前任务
    call exit_current_task
    ; 平栈
    pop ecx
    pop eax
    ; 返回
    ret

.next_scheduler:
    ; 结束当前任务，开始下一次调度
    call exit_current_task

.ready_scheduler:
    hlt ; 释放控制权，如果再被时钟中断唤醒到此处，则模拟中断调用流程

    push msg_start_next_scheduler
    call printk

    ; 相当于自动走时钟中断的逻辑
    cli
    call clock_task_scheduler
    jmp .ready_scheduler

