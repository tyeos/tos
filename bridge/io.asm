[bits 32]
[SECTION .text]

; -----------------------
; 按字节读入
; -----------------------
; al = inb(uint32 port)
; -----------------------
global inb
inb:
    push ebp
    mov ebp, esp
    xor eax, eax
    mov edx, [ebp + 8]      ; port
    in al, dx
    leave
    ret

; -----------------------
; 按字节写出
; -----------------------
; outb(uint32 port, uint32 value)
; -----------------------
global outb
outb:
    push ebp
    mov ebp, esp
    mov edx, [ebp + 8]      ; port
    mov eax, [ebp + 12]     ; value
    out dx, al
    leave
    ret

; -----------------------
; 按字读入
; -----------------------
; ax = inw(uint32 port)
; -----------------------
global inw
inw:
    push ebp
    mov ebp, esp
    xor eax, eax
    mov edx, [ebp + 8]      ; port
    in ax, dx
    leave
    ret

; -----------------------
; 按字写出
; -----------------------
; outw(uint32 port, uint32 value)
; -----------------------
global outw
outw:
    push ebp
    mov ebp, esp
    mov edx, [ebp + 8]      ; port
    mov eax, [ebp + 12]     ; value
    out dx, ax
    leave
    ret