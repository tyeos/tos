ORG 0x7c00

[SECTION .data]
LOADER_ADDR equ 0x500
LOADER_SECTOR_START equ 1
LOADER_SECTOR_NUM equ 4

[SECTION .text]
[BITS 16]
global _start
_start:
    ; -----------------------------------------------------
    ; 初始化段寄存器
    ; -----------------------------------------------------
    ; 主板通电，会进入到内存0xFFFF0执行第一行指令:
    ;   jmp f000:e05b
    ;   即正式进入BIOS程序（其中包括初始化中断向量表、检测硬件等一系列初始化操作）
    ; BIOS执行结束后，会执行交接指令：
    ;   jmp 0:0x7c00
    ;   即正式进入MBR程序，所以此时cs=0
    ; -----------------------------------------------------
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; -----------------------------------------------------
    ; 10H中断 0号功能 设置显示模式（可达到清屏的效果）
    ; -----------------------------------------------------
    ; 输入：
    ;   AH => 功能号
    ;   AL => 显示模式
    ;        ----------------------------
    ;        | AL | 模式 |  分辨率  | 颜色 |
    ;        |---------------------------|
    ;        | 00 | 文字 | 40*25   |  2  |
    ;        | 01 | 文字 | 40*25   |  16 |
    ;        | 02 | 文字 | 80*25   |  2  |
    ;        | 03 | 文字 | 80*25   |  16 |
    ;        | 04 | 图形 | 320*200 |  2  |
    ;        | 05 | 图形 | 320*200 |  4  |
    ;        | 06 | 图形 | 640*200 |  2  |
    ;        ----------------------------
    ; -----------------------------------------------------
    ; 输出：
    ;   AL => 显示模式标志/CRT控制模式字节
    ; ----------------------------------
    mov ax, 3
    int 0x10

    ; 将loader从硬盘读入内存
    mov di, LOADER_ADDR
    mov ecx, LOADER_SECTOR_START
    mov bl, LOADER_SECTOR_NUM
    call read_hd

    ; 跳转到loader
    mov si, msg
    call print
    jmp LOADER_ADDR

; -----------------------------------------------------
; 读取硬盘n个扇区，装载到指定内存中
; -----------------------------------------------------
; di => 将数据写入的内存地址
; ecx => LBA起始扇区号
; bl => 读入的扇区数
; -----------------------------------------------------
read_hd:
    ; -----------------------------------------------------
    ; 硬盘控制器主要端口寄存器说明：
    ;   ---------------------------------------------
    ;   | IO端口Primary通道 | 读端口用途    | 写端口用途 |
    ;   | 0x1F0            |          Data          |
    ;   | 0x1F1            |  Error      | Features |
    ;   | 0x1F2            |      Sector count      |
    ;   | 0x1F3            |      LBA low (0~7)     |
    ;   | 0x1F4            |      LBA mid (8~15)    |
    ;   | 0x1F5            |      LBA high (16~23)  |
    ;   | 0x1F6            |         Device         |
    ;   | 0x1F7            |  Status     | Command  |
    ;   ---------------------------------------------
    ; 端口写指令：
    ;   out dx, ax/al ; dx为端口，写到端口的数据用ax/al存放
    ; 端口读指令：
    ;   in ax/al, dx ; dx为端口，ax/al用来存储读出的数据
    ; 说明：
    ;   Data寄存器宽度16位，其他均为8位，使用ax还是al取决于端口位宽
    ; -----------------------------------------------------
    ; Device寄存器结构：
    ;   -------------------------------------------------
    ;   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
    ;   |  1  | MOD |  1  | DEV |    LBA地址的24~27位    |
    ;   -------------------------------------------------
    ; MOD:
    ;   寻址模式，0-CHS，1-LBA
    ; DEV:
    ;   选择磁盘，0-主盘（master），1-从盘（slave）
    ; -----------------------------------------------------
    ; Command = 0x20 为读盘命令，Command命令执行前其他参数应设置完毕
    ; -----------------------------------------------------

    ; 设置要读取的扇区数
    mov dx, 0x1f2
    mov al, bl
    out dx, al

    ; 设置LBA低8位
    mov eax, ecx
    inc dx
    out dx, al

    ; 设置LBA中8位
    shr eax, 8
    inc dx
    out dx, al

    ; 设置LBA高8位
    shr eax, 8
    inc dx
    out dx, al

    ; 设置LBA顶4位，及读盘模式
    shr eax, 8
    and al, 0x0f ; al高四位置0，al低四位为LBA第24~27位
    or al, 0xe0 ; al高四位设置为1110，表示LBA模式读取主盘
    inc dx
    out dx, al

    ; 发送读盘命令
    inc dx ; 0x1F7
    mov al, 0x20
    out dx, al

    ; -----------------------------------------------------
    ; 检测硬盘状态
    ; -----------------------------------------------------
    ; Status寄存器结构：
    ;   -------------------------------------------------
    ;   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
    ;   | BSY | RDY | WFT | SKC | DRQ | COR | IDX | ERR |
    ;   -------------------------------------------------
    ; BSY：硬盘是否繁忙
    ; DRQ：是否正在等待写入数据或等待读出数据
    ; RDY：控制器是否已准备好接收命令
    ; WFT：是否存在写故障
    ; ERR：是否有错误发生
    ; -----------------------------------------------------
    ; 注：一次检测只保证一个扇区
    ; -----------------------------------------------------
.check:
    mov dx, 0x1f7
    in al, dx ; 读硬盘状态
    and al, 0x88 ; 第3位为1表示已准备好数据，第7位为1表示硬盘忙
    cmp al, 0x08 ; 判断是否准备好数据
    jnz .check ; 没准备好继续循环

    ; 准备就绪，开始读盘
    mov dx, 0x1f0
    mov cx, 256 ; 一个扇区512字节，每次读一个字(2字节)
.read:
    in ax, dx
    mov [di], ax ; 装载到指定内存区域
    add di, 2
    loop .read

    ; 一个扇区读取完成，准备检测下一个扇区
    dec bl
    ja .check ; bl大于0则继续检测
    ret

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

msg db "ready jump to loader...", 10, 13, 0

TIMES 510-($-$$) DB 0
DW 0xAA55