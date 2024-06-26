ORG 0x500

; ------------------------------------------------------------------------------------------------------
; 定义 GDT 描述符属性
; ------------------------------------------------------------------------------------------------------
; 段描述符格式：
;    总占8字节，以下高低32位为人为划分，其实就是一段连续的8字节空间。
;    其中，段基址32位，位于低32位的16~31位，高32位的0~7和24~31位。
;    段界限20位（寻址范围范围1MB或4GB），位于低32位的0~15位，高32位的16~19位。
; 高32位：
;     -------------------------------------------------------------------------------------------------
;     | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  |
;     |                 段基址的24~31位                 |  G  | D/B |  L  | AVL |      段界限的16~19位   |
;     -------------------------------------------------------------------------------------------------
;     -------------------------------------------------------------------------------------------------
;     | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
;     |  P  |    DPL    |  S  |         TYPE          |               段基址的16~23位                   |
;     -------------------------------------------------------------------------------------------------
; 低32位：
;     -------------------------------------------------------------------------------------------------
;     | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  |
;     |                                          段基址的0~15位                                         |
;     -------------------------------------------------------------------------------------------------
;     -------------------------------------------------------------------------------------------------
;     | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
;     |                                          段界限的0~15位                                         |
;     -------------------------------------------------------------------------------------------------
; ------------------------------------------------------------------------------------------------------
; G:
;     定义段界限粒度：0-单位1B（即段界限1M），1-单位4K（即段界限4G）
; D/B:
;     代码段（使用D位）：
;        0-指令有效地址和草数据为16位，指令使用IP寄存器。
;        1-指令有效地址和草数据为32位，指令使用EIP寄存器。
;     栈段（使用B位）：
;        0-使用SP寄存器，栈的起始地址为16位寄存器的最大寻址范围0xFFFF。
;        1-使用ESP寄存器，栈的起始地址为32位寄存器的最大寻址范围0xFFFFFFFF。
; L:
;     保留位：0-32位代码段，1-64位代码段。
; AVL:
;     Available, 硬件对其没有专门的用途，操作系统可随意使用。
; P:
;     Present, 0-不存在于内存中（段异常），1-段存在于内存中
;     CPU检测到P值为0时，会转到异常处理程序，在处理完成后要将其置为1
; DPL:
;     Descriptor Privilege Level, 即段描述符特权级，0~3四种特权级，数字越小特权级越大。
;     一般情况下，操作系统处于最高的0特权级，用户程序处于最低的3特权级，某些指令仅在0特权级下才能执行。
;     对于受访者为代码段（只有特权级转移才用得到）：
;         如果目标为非一致性代码段，要求：数值上CPL=RPL=DPL，即只能平级访问。
;         如果目标为一致性代码段，要求：数值上CPL≥DPL && RPL≥DPL，即只能平级或向上级访问。
;         注：DPL位于目标段描述符中, RPL位于选择子中, CPL位于CS中。
;     对于受访者为数据段：
;         要求：数值上CPL≤DPL && RPL≤DPL，即只能平级或向下级访问。
; S:
;     0-系统段（也叫代码段，硬件相关），1-非系统段（也叫数据段，软件相关，包括操作系统）
; TYPE:
;     系统段结构（S=0）：
;        略
;     非系统段结构（S=1）：
;        以下结构中的通用参数说明：
;            第0位A位，表示Accessed位，由CPU设置，每当该段被CPU访问后CPU就将其置1，新建段描述符时应将其置零
;            第3位X位，表示Executable位，标识是否是可执行的代码，1为可执行
;        内存段类型=代码段(X=1):
;            -------------------------
;            |  3  |  2  |  1  |  0  |
;            -------------------------
;            |  X  |  C  |  R  |  A  |
;            ------------------------- --- 说明 ---
;            |  1  |  0  |  0  |  *  |   只执行代码段
;            |  1  |  0  |  1  |  *  |   可执行、可读代码段
;            |  1  |  1  |  0  |  *  |   可执行、一致性代码段
;            |  1  |  1  |  1  |  *  |   可执行、可读、一致性代码段
;            ------------------------- -------------------------
;            部分参数说明：
;               C: conforming，一致性代码段，也称依从代码段，即作为转移的目标段时，特权级是否和转移前的低特权级保持一致
;            ---------------------------------------------------
;        内存段类型=数据段(X=0):
;            -------------------------
;            |  3  |  2  |  1  |  0  |
;            -------------------------
;            |  X  |  E  |  W  |  A  |
;            ------------------------- --- 说明 ---
;            |  0  |  0  |  0  |  *  |   只读数据段
;            |  0  |  0  |  1  |  *  |   可读写数据段
;            |  0  |  1  |  0  |  *  |   只读，向下扩展的数据段
;            |  0  |  1  |  1  |  *  |   可读写，向下扩展的数据段
;            ------------------------- -------------------------
;            部分参数说明：
;               W: Writable，段是否可写
;               E: Extend，标识段的扩展方向，0为向上扩展（通常用于代码段和数据段），1为向下（通常用于栈段）。
;                           该扩展方向只是为了指导段界限检测的方式，和使用push指令时SP的方向无关。
;                           对于向上扩展的段，实际的段界限是段内可以访问的最后一字节。
;                           对于向下扩展的段，实际的段界限是段内不可以访问的第一个字节。
;                           向上扩展的检测方式为: 偏移地址+数据长度-1<=实际段界限大小
;                           向下扩展的检测方式为: 实际段界限大小<esp-操作数大小<=栈指针最大可访问地址（如B位为1时即32位最大可访问地址为0xFFFFFFFF）
;            ---------------------------------------------------
;        段寄存器使用时的段类型检查:
;           代码段(X=1)：
;               ----------------------
;               | TYPE | R=1  | R=1  |
;               ----------------------
;               |  CS  |     ok      |
;               |  DS  |  no  |  ok  |
;               |  ES  |  no  |  ok  |
;               |  FS  |  no  |  ok  |
;               |  GS  |  no  |  ok  |
;               |  SS  |     no      |
;               ----------------------
;           数据段(X=0)：
;               ----------------------
;               | TYPE | W=0  | W=1  |
;               ----------------------
;               |  CS  |     no      |
;               |  DS  |     ok      |
;               |  ES  |     ok      |
;               |  FS  |     ok      |
;               |  GS  |     ok      |
;               |  SS  |  no  |  ok  |
;               ----------------------
; ------------------------------------------------------------------------------------------------------
[SECTION .gdt]

DESC_G_4k equ 1<<23
DESC_D_32 equ 1<<22
DESC_L    equ 0<<21
DESC_AVL  equ 0<<20
; 定义四种类型的LIMIT（段界限的第二段，16~19位）
DESC_LIMIT_2_CODE   equ 0xF << 16
DESC_LIMIT_2_DATA   equ 0xF << 16
DESC_LIMIT_2_STACK  equ 0x0 << 16
DESC_LIMIT_2_SCREEN equ 0x0 << 16

DESC_P equ 1<<15
; 定义四种特权级
DESC_DPL_0 equ 00b<<13
DESC_DPL_1 equ 01b<<13
DESC_DPL_2 equ 10b<<13
DESC_DPL_3 equ 11b<<13

DESC_S_NOT_SYS equ 1<<12
; 定义四种类型的TYPE（SCREEN按DATA处理）
DESC_TYPE_CODE   equ 1000b<<8 ; X=1, R=0, C=0, A=0，代码段可执行，非一致性，不可读，访问位清零
DESC_TYPE_DATA   equ 0010b<<8 ; X=0, E=0, W=1, A=0，数据段不可执行，向上扩展，可写，访问位清零
DESC_TYPE_STACK  equ 0110b<<8 ; X=0, E=1, W=1, A=0，栈段不可执行，向下扩展，可写，访问位清零
DESC_TYPE_SCREEN equ DESC_TYPE_DATA ; 屏幕段和数据段保持一致

; ------------------------------------------------------------------------------------------------------
; 整合四种类型的高四字节和低四字节
; 屏幕段基址为0xb8000, 其他段基址为0
; 屏幕段界限为0x7fff>>12, 栈段界限为0, 其他段界限为(4G-1)>>12
; ------------------------------------------------------------------------------------------------------
DESC_CODE_HIGH4   equ (0x00<<24) + DESC_G_4k + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_2_CODE   + DESC_P + DESC_DPL_0 + DESC_S_NOT_SYS + DESC_TYPE_CODE   + 0x00
DESC_DATA_HIGH4   equ (0x00<<24) + DESC_G_4k + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_2_DATA   + DESC_P + DESC_DPL_0 + DESC_S_NOT_SYS + DESC_TYPE_DATA   + 0x00
DESC_STACK_HIGH4  equ (0x00<<24) + DESC_G_4k + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_2_STACK  + DESC_P + DESC_DPL_0 + DESC_S_NOT_SYS + DESC_TYPE_STACK  + 0x00
DESC_SCREEN_HIGH4 equ (0x00<<24) + DESC_G_4k + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_2_SCREEN + DESC_P + DESC_DPL_0 + DESC_S_NOT_SYS + DESC_TYPE_SCREEN + 0x0B

DESC_CODE_LOW4   equ 0x0000FFFF
DESC_DATA_LOW4   equ 0x0000FFFF
DESC_STACK_LOW4  equ 0x00000000
DESC_SCREEN_LOW4 equ 0x80000007 ; 段基址=0xb8000，粒度1B的情况下段界限0xbffff-0xb8000=0x7fff，粒度4K的情况下段界限0x7fff/4K=0x7fff>>12=0x7

    ; ------------------------------------------------------------------------------------------------------
    ; ---------------------------------------   构建GDT及其内部描述符   ---------------------------------------
    ; ------------------------------------------------------------------------------------------------------
GDT_BASE: ; index=0, 第0个描述符为空描述符
    dd 0x00000000
    dd 0x00000000
GDT_CODE: ; index=1, 定义代码 段描述符
    dd DESC_CODE_LOW4
    dd DESC_CODE_HIGH4
GDT_DATA: ; index=2, 定义数据 段描述符
    dd DESC_DATA_LOW4
    dd DESC_DATA_HIGH4
GDT_STACK: ; index=3, 定义栈 段描述符
    dd DESC_STACK_LOW4
    dd DESC_STACK_HIGH4
GDT_SCREEN: ; index=4, 定义屏幕 段描述符
    dd DESC_SCREEN_LOW4
    dd DESC_SCREEN_HIGH4

; ------------------------------------------------------------------------------------------------------
; 定义gdtr指向的48位内存数据结构：
;     -------------------------------------------------------------------------------
;     | 47  |  ...    ...    ...    ...    ...  |  16  |    15  |  ...   ...  |  0  |
;     |                GDT内存起始地址                   |            GDT界限          |
;     -------------------------------------------------------------------------------
; ------------------------------------------------------------------------------------------------------
GDT_SIZE equ $ - GDT_BASE
GDT_LIMIT equ GDT_SIZE - 1
gdt_ptr:
    dw GDT_LIMIT
    dd GDT_BASE


; ------------------------------------------------------------------------------------------------------
; 定义 Selector (选择子) 属性
; ------------------------------------------------------------------------------------------------------
; 结构：
;     -------------------------------------------------------------------------------------------------
;     | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
;     |                                     描述符索引值                              | TI  |    RPL    |
;     -------------------------------------------------------------------------------------------------
; ------------------------------------------------------------------------------------------------------
; RPL:
;     Request Privilege Level, 请求特权级(代表真正请求者的特权级，即真正资源需求者的CPL)，0~3
;     在任意时刻，当前特权级CPL(Current Privilege Level)保存在CS选择子中的RPL部分（其他段寄存器中选择子的RPL与CPL无关）.
;     代码段的执行发生转移时，处理器特权级检查的条件通过后，新代码段的DPL就变成了处理器的CPL, 即保存在代码段寄存器CS中的RPL位(一致性代码段除外)。
;     即代码段的低特权级(Before CPL)提升为高特权级(After CPL)后，RPL中保留原特权级(Before CPL)，
;       之后再访问数据段时，会同时校验 (After)CPL 和 RPL(Before CPL), 即CPL和RPL的权限必须同时大于等于受访者的DPL，
;       从而保证低特权级请求者不能随意修改高特权级下数据段的内容。
; TI:
;     Table Indicator, 0-在GDT中索引描述符，1-在LDT中索引描述符
;     注：LDT用在多任务的时候，Intel建议为每个程序单独赋予一个结构存储起私有资源，这个结构就是LDT，描述符本身就是描述一段内存的归属，
;         因为私有，所以位置不固定，即每加入一个任务都需要在GDT中添加新的LDT描述符，还要（使用lldt指令）重新加载LDTR，比较麻烦，
;         所以这个项目并不打算使用LDT，后面会通过TSS来保存多任务的状态，即给每个任务关联一个任务状态段（Task State Segment）来表示任务。
; 描述符索引值:
;     规定GDT中的第0个描述符是空描述符，如果选择子的索引值为0则会引用到它。
;     所以，不允许往CS和SS段寄存器中加载索引值为0的选择子。
;     虽然可以往DS、ES、FS、GS寄存器中加载值为0的选择子，但真正在使用时CPU将会抛出异常，毕竟第0个段描述符是哑的，不可用。
;     如果索引值超出描述符表（是GDT还是LDT由TI位决定）定义的界限值也会抛出异常。
; ------------------------------------------------------------------------------------------------------
[SECTION .selector]

; 定义四种特权级
RPL_0 equ 00b
RPL_1 equ 01b
RPL_2 equ 10b
RPL_3 equ 11b

TI_GDT equ 000b
TI_LDT equ 100b

; --------------------------------------------
; ---------------   定义选择子   ---------------
; --------------------------------------------
SELECTOR_CODE   equ 1<<3 + TI_GDT + RPL_0
SELECTOR_DATA   equ 2<<3 + TI_GDT + RPL_0
SELECTOR_STACK  equ 3<<3 + TI_GDT + RPL_0
SELECTOR_SCREEN equ 4<<3 + TI_GDT + RPL_0


[SECTION .data]
HEAD_ADDR equ 0x1500
HEAD_SECTOR_START equ 5
HEAD_SECTOR_NUM equ 128

; 用于存储内存检测的数据
ARDS_TIMES_SAVE_ADDR equ 0xE00 ; 最终检测次数存放地址，占2字节
ARDS_BUFFER equ 0x1000 ; 每次检测返回的ARDS占用20个字节的情况下，第N段ARDS的起始位置为: 0x1000 + N * 0x14
ARDS_TIMES dw 0 ; 临时检测次数

CHECK_BUFFER_OFFSET dw 0 ; 存储填充以后的offset，下次检测的结果接着写

init_msg:
    db "breaking through 512 bytes!", 10, 13, 0

memory_check_error_msg:
    db "memory check error!", 10, 13, 0

memory_check_success_msg:
    db "memory check success!", 10, 13, 0

[SECTION .text]
[BITS 16]
global loader_start
loader_start:
    ; 初始化段寄存器
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov     si, init_msg
    call    print

; ----------------------------------------------------------------------------------------------------------------------
; 内存检测
; ----------------------------------------------------------------------------------------------------------------------
; BIOS 15H中断 0xE820号功能: 遍历主机上全部内存
; ----------------------------------------------------------------------------------------------------------------------
; BIOS中断0x15的子功能0xE820能够获取系统的内存布局
; 由于系统内存各部分的类型属性不同，BIOS就按照类型属性来划分这片系统内存，
; 所以这种查询呈迭代式，每次BIOS只返回一种类型的内存信息，直到将所有内存类型返回完毕。
; 子功能0xE820的强大之处是返回的内存信息较丰富，包括多个属性字段，所以需要一种格式结构来组织这些数据。
; 内存信息的内容是用地址范围描述符来描述的，用于存储这种描述符的结构称之为地址范围描述符(Address Range Descriptor Structure, ARDS)。
;
; 地址范围描述符(Address Range Descriptor Structure, ARDS)格式：
;     ------------------------------------------------------------
;     | byte offset | attribute name | describe                  |
;     ------------------------------------------------------------
;     | 0           | BaseAddrLow    | 基地址的低32位              |
;     | 4           | BaseAddrHigh   | 基地址的高32位              |
;     | 8           | LengthLow      | 内存长度的低32位，以字节为单位 |
;     | 12          | LengthHigh     | 内存长度的高32位，以字节为单位 |
;     | 16          | Type           | 本段内存的类型               |
;     ------------------------------------------------------------
;     此结构中的字段大小都是4字节，共5个字段，所以此结构大小为20字节。
;     每次int 0x15之后，BIOS就返回这样一个结构的数据。
;     注意，ARDS结构中用64位宽度的属性来描述这段内存基地址（起始地址）及其长度，
;     所以表中的基地址和长度都分为低32位和高32位两部分。
;     其中的Type字段用来描述这段内存的类型，这里所谓的类型是说明这段内存的用途，
;     即其是可以被操作系统使用，还是保留起来不能用。
;
; ARDS结构的Type字段：
;     ----------------------------------------------------------------------------------------------------------------
;     | Type value | name                 | describe                                                                 |
;     ----------------------------------------------------------------------------------------------------------------
;     | 1          | AddressRangeMemory   | 这段内存可以被操作系统使用                                                    |
;     | 2          | AddressRangeReserved | 内存使用中或者被系统保留，操作系统不可以用此内存                                  |
;     | other      | undefined            | 未定义，将来会用到，目前保留。但是需要操作系统一样将其视为ARR(AddressRangeReserved) |
;     ----------------------------------------------------------------------------------------------------------------
; ----------------------------------------------------------------------------------------------------------------------
; 输入：
;     EAX   => 子功能号，此处输入为0xE820
;     EBX   => ARDS后续值：内存信息需要按类型分多次返回，由于每次执行一次中断都只返回一种类型内存的ARDS结构，所以要记录下一个待返回的内存ARDS,
;                 在下一次中断调用时通过此值告诉BIOS该返回哪个ARDS,这就是后续值的作用。第一次调用时一定要置为O。
;     ES:DI => ARDS缓冲区：BIOS将获取到的内存信息写入此寄存器指向的内存，每次都以ARDS格式返回
;     ECX   => ARDS结构的字节大小：用来指示BIOS写入的字节数。调用者和BIOS都同时支持的大小是20字节，将来也许会扩展此结构
;     EDX   => 固定为签名标记0x534d4150,此十六进制数字是字符串SMAP的ASCI码：
;                 BIOS将调用者正在请求的内存信息写入ES:DI寄存器所指向的ARDS缓冲区后，再用此签名校验其中的信息
; ----------------------------------------------------------------------------------------------------------------------
; 输出：
;     CF位  => 若CF位为0表示调用未出错，CF为1表示调用出错
;     EAX   => 字符串SMAP的ASCI码0x534d4150
;     ES:DI => ARDS缓冲区地址，同输入值是一样的，返回时此结构中已经被BIOS填充了内存信息
;     ECX   => BIOS写入到ES:DI所指向的ARDS结构中的字节数，BIOS最小写入20字节
;     EBX   => 后续值：下一个ARDS的位置。每次中断返回后，BIOS会更新此值，BIOS通过此值可以找到下一个待返回的ARDS结构，
;                  我们不需要改变EBX的值，下一次中断调用时还会用到它。在CF位为O的情况下，若返回后的EBX值为O,表示这是最后一个ARDS结构
; ----------------------------------------------------------------------------------------------------------------------
memory_check:
    xor ebx, ebx            ; ebx = 0
    mov di, ARDS_BUFFER     ; es:di 指向一块内存地址，es此时为0

.loop:
    mov eax, 0xE820         ; ax = 0xe820
    mov ecx, 20             ; ecx = 20
    mov edx, 0x534D4150     ; edx = 0x534D4150
    int 0x15

    jc memory_check_error   ; 检测CF位为1则跳转到错误处理

    add di, cx              ; di指向下一块内存地址, 由于di处于16位模式, 所以用cx就可以
    inc dword [ARDS_TIMES]  ; 检测次数 + 1
    cmp ebx, 0              ; ebx不为0就要继续检测
    jne .loop

    mov ax, [ARDS_TIMES]            ; 保存内存检测次数
    mov [ARDS_TIMES_SAVE_ADDR], ax
    mov [CHECK_BUFFER_OFFSET], di   ; 保存offset

.memory_check_success:
    mov si, memory_check_success_msg
    call print

    ; ------------------------------------------------------------------------------------------------------
    ; -----------------------------------------   准备进入保护模式   ------------------------------------------
    ; ------------------------------------------------------------------------------------------------------

    ; 关中断
    cli

    ; 开启A20
    in al, 0x92
    or al, 0000_0010b
    out 0x92, al

    ; 加载GDT
    lgdt [gdt_ptr]

    ; 设置保护模式开关（CR0第0位置1）
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax

    ; ------------------------------------------------------------------------------------------------------
    ; ------------------------   当前已开启保护模式（当前还是16位编码，即处于16位保护模式）   ------------------------
    ; ------------------------------------------------------------------------------------------------------
    jmp SELECTOR_CODE:protected_model_start   ; 无条件跳转可刷新流水线

; ------------------------
; 内存检测出现错误
; ------------------------
memory_check_error:
    mov     si, memory_check_error_msg
    call    print
    jmp $

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


; ------------------------------------------------------------------------------------------------------
; -----------------------------------   以下程序处于32位保护模式下执行   ------------------------------------
; ------------------------------------------------------------------------------------------------------
[bits 32]
protected_model_start:
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ax, SELECTOR_STACK
    mov ss, ax
    mov esp, 0x9FC00

    ; 将内核入口程序读入内存
    mov edi, HEAD_ADDR
    mov ecx, HEAD_SECTOR_START
    mov bl, HEAD_SECTOR_NUM
    call read_hd

    jmp SELECTOR_CODE:HEAD_ADDR

; -----------------------------------------------------
; 读取硬盘n个扇区，装载到指定内存中
; -----------------------------------------------------
; edi => 将数据写入的内存地址
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
    mov ecx, 256 ; 一个扇区512字节，每次读一个字(2字节)
.read:
    in ax, dx
    mov [edi], ax ; 装载到指定内存区域
    add edi, 2
    loop .read

    ; 一个扇区读取完成，准备检测下一个扇区
    dec bl
    ja .check ; bl大于0则继续检测
    ret
