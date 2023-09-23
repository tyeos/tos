[SECTION .data]
TSS_STRUCT_SIZE equ 27 << 2 ; tss_t 占用字节数

[SECTION .text]
[bits 32]

extern current_task ; 在c中定义，当前任务
extern task_scheduler_ticks ; 在c中定义，进行一次任务调度滴答
extern exit_current_task ; 在c中定义，退出普通任务，换到idle任务

; ---------------------------------------------------------------------------------------
; 时钟中断处理函数
; ---------------------------------------------------------------------------------------
; 初始栈结构（摘自 interrupt.asm）:
; -----------------------------------------------------
;     eip          ↑ 低地址（ <-esp 当前栈顶）
;     cs           ↑
;     eflags       ↑ （栈的增长方向）
;     (esp)        ↑ 注：从这里开始往下 ↓ 是发生态切换时额外压入的两个值
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

    ; ---------------------------------------------------------------------------------------
    ; 当前栈结构
    ; ---------------------------------------------------------------------------------------
    ;     ecx          ↑ 低地址（ <-esp 当前栈顶）
    ; ----------------------------------------
    ;     eip          ↑ （栈的增长方向）
    ;     cs           ↑
    ;     eflags       ↑
    ;     (esp)        ↑
    ;     (ss)         ↑ 高地址
    ; ---------------------------------------------------------------------------------------

    ; eip
    mov eax, [esp + 4]
    mov [ecx + 8 * 4], eax

    ; cs
    mov eax, [esp + 8]
    mov [ecx + 19 * 4], eax

    ; eflags
    mov eax, [esp + 0x0C]
    mov [ecx + 9 * 4], eax

    ; -------------------------------------------------------------------
    ; 这里要看是否是用户进程过来的，是的话还要保存r3的ss:esp, 并修复r0的esp
    ; -------------------------------------------------------------------
    and eax, 0x3
    jz .skip_store_user_env
    ; esp (r3)
    mov eax, [esp + 0x10]
    mov [ecx + 1 * 4], eax
    ; ss (r3)
    mov eax, [esp + 0x14]
    mov [ecx + 2 * 4], eax

.skip_store_user_env:

    ; 用eax把ecx换下来
    mov eax, ecx
    pop ecx ; 恢复 ecx 和 esp
    mov [eax + 11 * 4], ecx
    mov [eax + 14 * 4], esp

    ; 恢复eax
    mov eax, [ecx + 10 * 4]

    jmp .start_task_scheduler

.skip_store_env:
    pop ecx

.start_task_scheduler:
    ; ---------------------------------------------------------------------------------------
    ; 这里以上都是保存任务状态，到这里所有寄存器和栈都已恢复
    ; ---------------------------------------------------------------------------------------
    ; 开始进行一次任务调度滴答，会更新当前任务
    ; ---------------------------------------------------------------------------------------

    push eax
    call task_scheduler_ticks

    ; 检查任务
    mov eax, [current_task]
    cmp eax, 0
    je .exit ; 没有可调度任务，直接哪儿来的回哪儿去（idle任务会一直在，只有测试时才会让idle结束）

.processing_task:
    ; 开始处理任务，先保存要用的寄存器
    push ecx

    ; ---------------------------------------------------------------------------------------
    ; 当前栈结构
    ; ---------------------------------------------------------------------------------------
    ;     ecx          ↑ 低地址（ <-esp 当前栈顶）
    ;     eax          ↑
    ; ----------------------------------------
    ;     eip          ↑ （栈的增长方向）
    ;     cs           ↑
    ;     eflags       ↑
    ;     (esp)        ↑
    ;     (ss)         ↑ 高地址
    ; ---------------------------------------------------------------------------------------

    ; 检查是否首次调度
    mov ecx, [eax + TSS_STRUCT_SIZE + 3 * 4] ; 总调度次数
    cmp ecx, 1
    jne .recover_env ; 旧任务需恢复上下文环境 （暂不考虑调度次数超过0xFFFFFFFF后归0的情况, 10ms一次归零也需要497天）

.run_new_task:

    ; -------------------------------------------------------------------------
    ; 新任务执行
    ; -------------------------------------------------------------------------
    ; 每个内核任务都有自己的任务栈
    ; 保存当前任务esp，启用新任务esp
    ; 之后跳转到具体任务方法执行即可
    ; -------------------------------------------------------------------------

    pop ecx
    ; ---------------------------------------------------------------------------------------
    ; 当前栈结构
    ; ---------------------------------------------------------------------------------------
    ;     eax          ↑ 低地址（ <-esp 当前栈顶）
    ; ----------------------------------------
    ;     eip          ↑ （栈的增长方向）
    ;     cs           ↑
    ;     eflags       ↑
    ;     (esp)        ↑
    ;     (ss)         ↑ 高地址
    ; ---------------------------------------------------------------------------------------

    mov [eax + 26 * 4], esp                  ; ssp, 保存当前栈（只有0号任务有意义）
    mov esp, [eax + TSS_STRUCT_SIZE + 5 * 4] ; 切到任务自己的内核任务栈
    mov eax, [eax + TSS_STRUCT_SIZE + 4]     ; func
    sti                                      ; 允许中断
    call eax                                 ; 任务开始执行

.run_task_end:

    ; -------------------------------------------------------------------------
    ; 任务执行完成
    ; -------------------------------------------------------------------------
    ; 所有任务都是直接或间接由idle任务（0号任务）启动的，
    ; 除idle外，其他任务都可能会随时执行结束，
    ; 所以普通任务执行完毕，是不可以中断返回的，因为很可能回到已经被释放的错误地址，
    ; 而且，目前要结束当前任务，即当前任务栈也需要销毁，
    ; 所以，要有栈可用，要么恢复idle任务执行，要么建新任务用新任务栈，
    ; 这里为了统一流程，还是交由idle处理
    ; -------------------------------------------------------------------------
    cli ; 关中断，保证顺利到下一个流程
    push ecx ; 这里是暂存到任务的内核栈

    mov eax, [current_task]
    mov ecx, [eax + TSS_STRUCT_SIZE] ; pid
    cmp ecx, 0
    je .exit_idle_task ; idle 任务的终止，直接退回到main

    ; 普通任务终止
    call exit_current_task
    mov eax, [current_task] ; 后续即进入0号任务恢复流程

.recover_env:

    ; -------------------------------------------------------------------
    ; 恢复任务上下文。
    ; -------------------------------------------------------------------
    ; 这里的主要任务就是恢复到之前的任务，并且不再回来，
    ; 即栈要恢复到创建任务时的栈，代码要跳转该任务到第一次中断时的地方。
    ; -------------------------------------------------------------------

    mov edi, [eax + 17 * 4]
    mov esi, [eax + 16 * 4]
    mov ebp, [eax + 15 * 4]
    mov esp, [eax + 14 * 4] ; 当前esp不再使用，这里将esp退回到原栈，即任务执行结束后，栈会平到新任务执行结束处，而非回到这里的栈
    mov ebx, [eax + 13 * 4]
    mov edx, [eax + 12 * 4]
    ; mov ecx, [eax + 11 * 4]
    ; mov eax, [eax + 10 * 4]

    ; ---------------------------------------------------------------------------------------
    ; 手动模拟中断返回，即切换到原来的场景
    ; ---------------------------------------------------------------------------------------
    ; 要构建的栈结构：
    ; -----------------------------------------------------------
    ;     eip          ↑ 低地址（ <-esp 最终栈顶）
    ;     cs           ↑（栈的增长方向）
    ;     eflags       ↑ 高地址
    ;     (esp)        ↑ (用户程序还需要下面这两个)
    ;     (ss)         ↑ 高地址
    ; ---------------------------------------------------------------------------------------
    ; 注意，这个构建要保证在完成时，esp和现在的值相等，即完全还原到之前的状态，
    ; 因为在中断保存上下文时，本身esp的状态已经处于指向了压入3或5个寄存器的栈，
    ; 所以这里要从中断前模拟，即先退回esp，这样才是完全恢复
    ; ---------------------------------------------------------------------------------------
    add esp, 3 * 4  ; 先假设是压入3个寄存器

    ; -------------------------------------------------------------------
    ; 检查，如果是恢复用户进程的任务，还需要压入r3的ss和esp
    ; -------------------------------------------------------------------
    mov ecx, [eax + 19 * 4] ; cs
    and ecx, 0x3
    jz .skip_recover_user_env
    add esp, 2 * 4  ; 补充为压入5个寄存器
    ; ss
    mov ecx, [eax + 2 * 4]
    push ecx
    ; esp
    mov ecx, [eax + 1 * 4]
    push ecx

.skip_recover_user_env:

    ; eflags
    mov ecx, [eax + 9 * 4]
    push ecx
    ; cs
    mov ecx, [eax + 19 * 4]
    push ecx
    ; eip
    mov ecx, [eax + 8 * 4]
    push ecx

    ; ------------------------------------------------------------
    ; 栈侯建完成，恢复临时使用的所有寄存器
    ; ------------------------------------------------------------
    mov ecx, [eax + 11 * 4]
    mov eax, [eax + 10 * 4]

    ; 以中断返回的方式跳转，不会再回来了
    iret

.exit_idle_task:

    ; ------------------------------------------------------------
    ; 到这里还是用的任务的栈，并且eax和ecx还没有恢复
    ; ------------------------------------------------------------
    ; 先将ecx从任务的栈中恢复，
    ; 再结束任务，并返回原来的中断栈，用于恢复
    ; ------------------------------------------------------------

    pop ecx
    call exit_current_task   ; 返回 ssp 在 eax 中
    mov esp, eax

    ; ---------------------------------------------------------------------------------------
    ; 到这里已经恢复到中断栈了，只剩下eax没有恢复
    ; ---------------------------------------------------------------------------------------
    ; 当前栈结构
    ; ---------------------------------------------------------------------------------------
    ;     eax          ↑ 低地址（ <-esp 当前栈顶）
    ; ----------------------------------------
    ;     eip          ↑ （栈的增长方向）
    ;     cs           ↑
    ;     eflags       ↑
    ;     (esp)        ↑
    ;     (ss)         ↑ 高地址
    ; ---------------------------------------------------------------------------------------

.exit:
    pop eax
    iret

