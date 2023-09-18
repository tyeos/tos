[SECTION .text]
[BITS 32]

extern exit_current_task ; 在c中定义，退出任务

; ---------------------------------
; 开始处理调度的任务
; ---------------------------------
; void processing_task(task_t *)
; ---------------------------------
global processing_task
processing_task:
    ; ---------------------------------------------------------------------------------------
    ; 当前栈结构（C程序调用进来）
    ; ---------------------------------------------------------------------------------------
    ;     return addr  ↑ 低地址（ <-esp 当前栈顶）
    ;     task_t *     ↑ 高地址
    ; ---------------------------------------------------------------------------------------

    ; eax一般作为函数返回值，可以不用保存
    mov eax, [esp + 4] ; task

    ; 检查是否首次调度先
    push ecx ; 先保存
    mov ecx, [eax + 30 * 4] ; 总调度次数
    inc ecx
    mov [eax + 30 * 4], ecx ; 先把调度次数加了
    cmp ecx, 1
    je .skip_recover_env ; 首次调度无需恢复上下文环境 （暂不考虑调度次数超过0xFFFFFFFF后归0的情况, 10ms一次归零也需要497天）

.recover_env:
    ; -------------------------------------------------------------------
    ; 恢复上下文, 参考 interrupt_clock.asm 中的保存上下文
    ; -------------------------------------------------------------------

    mov edi, [eax + 17 * 4]
    mov esi, [eax + 16 * 4]
    mov ebp, [eax + 15 * 4]
    mov esp, [eax + 14 * 4]
    mov ebx, [eax + 13 * 4]
    mov edx, [eax + 12 * 4]
    ; mov ecx, [eax + 11 * 4]
    ; mov eax, [eax + 10 * 4] ; eax 作为跳转使用，目前不做恢复

.recover_run:
    ; eflags
    mov ecx, [eax + 9 * 4]
    push ecx
    popfd

    pop ecx ; 恢复ecx

    ; eip (cs暂不考虑)
    mov eax, [eax + 8 * 4]
    jmp eax ; 跳转到eip处继续执行

    jmp .exit

.skip_recover_env:
    pop ecx ; 恢复ecx

.run_task: ; 新任务执行
    mov eax, [eax + 28 * 4]

    sti
    call eax

.exit:
    call exit_current_task

    ret

