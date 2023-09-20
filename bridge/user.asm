[SECTION .data]
R3_CODE_GDT_ENTRY_INDEX equ 5
R3_DATA_GDT_ENTRY_INDEX equ 6

R3_CODE_SELECTOR equ R3_CODE_GDT_ENTRY_INDEX << 3 | 0b011
R3_DATA_SELECTOR equ R3_DATA_GDT_ENTRY_INDEX << 3 | 0b011

[SECTION .text]
[bits 32]

extern alloc_upage
extern user_entry

global move_to_user_mode
move_to_user_mode:
    ; ---------------------------------------------------------------------------------------
    ; 手动模拟中断返回，即切换到用户态
    ; ---------------------------------------------------------------------------------------
    ; 要构建的栈结构：
    ; ---------------------------------------------------------------------------------------
    ;     eip          ↑ 低地址（ <-esp 最终栈顶）
    ;     cs           ↑
    ;     eflags       ↑ （栈的增长方向）
    ;     esp          ↑
    ;     ss           ↑ 高地址
    ; ---------------------------------------------------------------------------------------

    call alloc_upage
    add eax, 0x1000

    push R3_DATA_SELECTOR
    push eax
    pushfd
    push R3_CODE_SELECTOR
    push user_entry

    iret