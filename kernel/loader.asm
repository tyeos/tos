ORG 0x500

[SECTION .data]
msg:
    db "breaking through 512 bytes!", 10, 13, 0

[SECTION .text]
[BITS 16]
global _loader
_loader:
    ; 初始化段寄存器
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov     si, msg
    call    print

    jmp     $

; ------------------------
; 打印字符串
; ------------------------
; 参数：
;   si => 字符串起始地址（以\0标识字符串结尾）
; ------------------------
print:
    ; -----------------------------------------------------
    ; 10H中断 EH号功能 输出字符(光标跟随字符移动)
    ; -----------------------------------------------------
    ; 输入：
    ;   AH => 功能号
    ;   AL => 字符
    ;   BH => 页码
    ;   BL => 颜色（只适用于图形模式）
    ; -----------------------------------------------------
    ; 输出：
    ;   无
    ; ----------------------------------
    mov ah, 0x0e
    mov bx, 0x0001
.loop:
    mov al, [si]
    cmp al, 0
    jz .done
    int 0x10
    inc si
    jmp .loop
.done:
    ret
