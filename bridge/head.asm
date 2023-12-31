; 该文件使用elf格式编译，所以不用加ORG声明
[SECTION .text]
[BITS 32]

extern kernel_main

global _start
_start:
; ----------------------------------------------------------------------------------------------------------------------
; 配置 8259A 中断代理芯片 （相关芯片有 ICW1~ICW4 和 OCW1~OCW3） ：
;     ICW1和OCW2、OCW3是用偶地址端口0x20(主片)或0xA0(从片)写入。
;     ICW2~ICW4和OCW1是用奇地址端口0x21(主片)或0xA1(从片)写入。
;     以上4个ICW要保证一定的次序写入(这样8259A才知道写入端口的数据是什么)。
;     OCW无需保证顺序(8259A会识别ICW1和OCW2、OCW3控制字中的标识)。
; ----------------------------------------------------------------------------------------------------------------------
; ICW1:
;     ICW1需要写入到主片的0x20端口和从片的0xA0端口
; 格式:
;     -------------------------------------------------
;     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
;     |  0  |  0  |  0  |  1  |LTIM | ADI |SNGL | IC4 |
;     -------------------------------------------------
; IC4:
;     表示是否要写入ICW4, 并不是所有的ICW初始化控制字都需要用到。
;     IC4为1时表示需要在后面写入ICW4, 为0则不需要。
;     x86系统IC4必须为1。
; SNGL:
;     表示single, SNGL为1表示单片，SNGL为O表示级联(cascade)。
;     在级联模式下，这要涉及到主片(1个)和从片（多个）用哪个IRQ接口互相连接的问题，
;     所以当SNGL为为0时，主片和从片也是需要ICW3的。
; ADI:
;     表示call address interval, 用来设置8085的调用时间间隔，x86不需要设置。
;     1表示4字节中断向量，0表示8字节中断向量（即中断描述符的长度）。
; LTIM:
;     表示level/edge triggered mode, 用来设置中断检测方式，LTIM为0表示边沿触发，LTIM为1表示电平触发。
; 第4位：
;     固定为1，这是ICW1的标记
; 5~7位：
;     专用于8085处理器，x86不需要，直接置为0即可。
;
; ----------------------------------------------------------------------------------------------------------------------
; ICW2：
;     用来设置起始中断向量号(IRQ0映射到的中断向量号)，每个8259A芯片上的IRQ接口是顺序排列的，
;     ICW2需要写入到主片的0x21端口和从片的0xA1端口。
; 格式:
;     -------------------------------------------------
;     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
;     |  T7 |  T6 |  T5 |  T4 |  T3 | ID2 | ID1 | ID0 |
;     -------------------------------------------------
; T3~T7:
;     起始中断向量号
; D0~D2:
;     不用处理，低3位能表示8个中断向量号,
;     由8259A根据8个IRQ接口的排列位次自行导入，RQ0的值是000，RQ1的值是001，RQ2的值便是010…以此类推。
;     高5位加低3位，可表示任意一个IRQ接口实际分配的中断向量号。
;
; ----------------------------------------------------------------------------------------------------------------------
; ICW3：
;     仅在级联的方式下才需要（ICW1中的SNGL为0），用来设置主片和从片用哪个IRQ接口互连
;     ICW3需要写入主片的0x21端口及从片的0xA1端口。
;     ICW3主片和从片有不同的结构
; 主片格式:
;     -------------------------------------------------
;     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
;     |  S7 |  S6 |  S5 |  S4 |  S3 |  S2 |  S1 |  S0 |
;     -------------------------------------------------
; S0~S7:
;     置1表示对应的IRQ接口用于连接从片，为0则表示接外部设备
; 从片格式:
;     -------------------------------------------------
;     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
;     |  0  |  0  |  0  |  0  |  0  | ID2 | ID1 | ID0 |
;     -------------------------------------------------
; D0~D2:
;     能表示自己和主片的0~7哪个IRQ连接。
;     中断响应时，主片发送与从片做级联的IRQ接口号，所有从片用自己的ICW3低3位和它对比，若一致则认为是发给自己的。
;
; ----------------------------------------------------------------------------------------------------------------------
; ICW4:
;     有些低位的选项基于高位。
; 格式:
;     -------------------------------------------------
;     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
;     |  0  |  0  |  0  |SFNM | BUF | M/S |AEOI | μPM |
;     -------------------------------------------------
; 第7~5位：
;     未定义，置为0即可。
; SFNM：
;     Special Fully Nested Mode, 为O表示全嵌套模式，为1表示特殊全嵌套模式。
; BUF：
;     表示本8259A芯片是否工作在缓冲模式。0为工作非缓冲模式，1为工作在缓冲模式。
; M/S:
;     多个8259A级联时，如果工作在缓冲模式下，M/S用来规定本8259A是主片(M/S为1)，还是从片(M/S为0)。
;     若工作在非缓冲模式(BUF为0)下，M/S无效。
; AEOI:
;     表示自动结束中断(Auto End Of Interrupt),
;     8259A在收到中断结束信号时才能继续处理下一个中断，此项用来设置是否要让8259A自动把中断结束。
;     若AEOI为0，则表示非自动，即手动结束中断（通过OCW可手动向8259A的主、从片发送EOI信号）
;     若AEOI为1，则表示自动结束中断。
; μPM:
;     表示微处理器类型(microprocessor), 此项是为了兼容老处理器。
;     若uPM为0，则表示8080或8085处理器，若PM为1，则表示x86处理器。
;
; ----------------------------------------------------------------------------------------------------------------------
; OCW1：
;     用来屏蔽连接在8259A上的外部设备的中断信号，实际上就是把OCW1写入了IMR寄存器。
;     这里的屏蔽是说是否把来自外部设备的中断信号转发给CPU。
;     OCW1要写入主片的0x21或从片的0xA1端口
; 格式:
;     -------------------------------------------------
;     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
;     |  M7 |  M6 |  M5 |  M4 |  M3 |  M2 |  M1 |  M0 |
;     -------------------------------------------------
; M0~M7:
;     对应8259A的IRQ0~IRQ7, 值为1对应的IRQ上的中断信号被屏蔽, 为0对应的IRQ中断信号放行。
;
; ----------------------------------------------------------------------------------------------------------------------
; OCW2:
;     用来设置中断结束方式和优先级模式。
;     OCW2要写入主片的0x20或从片的0xA0端口
; 格式:
;     -------------------------------------------------
;     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
;     |  R  |  SL | EOI |  0  |  0  |  L2 |  L1 |  L0 |
;     -------------------------------------------------
; R:
;     Rotation,表示是否按照循环方式设置中断优先级。
;     R为0表示不自动循环,采用固定优先级(IRQ接口号越低,优先级越高)方式。
;         IR0>IR1>IR2>IR3>IR4>IR5>IR6>IR7
;         IRQ各接口在此被表述为“IR数字”的形式，这也是微机接口中的命名规则。
;     R为1表示优先级自动循环(中断被处理完成后，它的优先级别将变成最低)。
;         -----------------------------------------
;         | IR5>IR6>IR7>IR0>IR1>IR2>IR3>IR4(>IR5) | 如：此时IR5位最低优先级
;         |  ↑________________________________↓   |
;         -----------------------------------------
; SL:
;     Specific Level,表示是否指定优先等级。等级是用低3位来指定的。
;     此处的SL只是开启低3位的开关，所以SL也表示低3位的L2~L0是否有效。SL为1表示有效，SL为0表示无效。
;     当R为1时：
;         若SL为0，初始的优先级次序为IR0>IR1>IR2>IR3>IR4>IR5>IR6>IR7。
;         若SL为1，且L2~L0值为101（即指定IR5为最低优先级），则初始优先级循环为IR6>IR7>IR0>IR1>IR2>IR3>IR4>IR5。
; EOI:
;     End Of Interrupt,为中断结束命令位。
;     EOI为1,则会令ISR寄存器中的相应位清0，也就是将当前处理的中断清掉，表示处理结束。
;     向8259A主动发送EOI是手工结束中断的做法，所以，使用此命令有个前提，就是ICW4中的AEOI位为0，非自动结束中断时才用。
;     在手动结束中断(AEOI位为0)的情况下，如果中断来自主片，只需要向主片发送EOI就行了，
;         如果中断来自从片，除了向从片发送EOI以外，还要再向主片发送EOI。
; 第4~3位：
;     00是OCW2的标识
; L2~L0:
;     用来确定优先级的编码，这里分两种，一种用于E0I时，表示被中断的优先级别，另一种用于优先级循环时，指定起始最低的优先级别。
; OCW2高位组合属性：
;     --------------
;     | R | SL |EOI|
;     -------------- ---描述-----------------------------------------------------------------------------------------
;     | 0 | 0  | 1 | 普通EOI结束方式：当中断处理完成后，向8259A发送EOI命令，8259A将ISR中当前级别最高的位置0
;     | 0 | 1  | 1 | 特殊EOI结束方式：当中断处理完成后，向8259A发送EOI命令，8259A将ISR寄存器中由L2~L0指定的位清0
;     | 1 | 0  | 1 | 普通EOI循环命令：当中断处理完成后，8259A将ISR中当前优先级最高的位清0，并使此位的优先级变成最低，其他优先级变更依次类推
;     | 1 | 1  | 1 | 特殊EOI循环命令：当中断处理完成后，8259A将ISR中由L2~L0指定的相应位清0，并使此位的优先级变成最低，其他优先级变更依次类推
;     | 0 | 0  | 0 | 清除自动EOI循环命令
;     | 1 | 0  | 0 | 设置自动EOI循环命令：8259A自动将ISR寄存器中当前处理的中断位清0，并使此位的优先级变成最低，其他优先级变更依次类推
;     | 1 | 1  | 0 | 设置优先级命令：将L2~L0指定的R(i+1)为最低优先级，R(+1)为最高优先级，其他优先级依次类推
;     | 0 | 1  | 0 | 无操作
;     -------------- ------------------------------------------------------------------------------------------------
;
; ----------------------------------------------------------------------------------------------------------------------
; OCW3：
;     用来设定特殊屏蔽方式及查询方式
;     OCW3要写入主片的0x20端口或从片的0xA0端口。
; 格式:
;     -------------------------------------------------
;     |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
;     |  /  |ESMM | SMM |  0  |  1  |  P  |  PR | RIS |
;     -------------------------------------------------
;
; 第7位:
;     未用到
; ESMM、SMM:
;     ESMM(Enable Special Mask Mode)和SMM(Special Mask Mode)组合使用，用来启用或禁用特殊屏蔽模式。
;     ESMM是特殊屏蔽模式允许位，是个开关。
;     SMM是特殊屏蔽模式位。
;     只有在启用特殊屏蔽模式时，特殊屏蔽模式才有效。
;         若ESMM为0，则SMM无效。
;         若ESMM为1，SMM为0，表示未工作在特殊屏蔽模式。
;         若ESMM和SMM都为1，这才正式工作在特殊屏蔽模式下。
; 第4~3位:
;     01是OCW3的标识，8259A通过这两位判断是哪个控制字。
; P：
;     Poll command,查询命令，当P为1时，设置8259A为中断查询方式，这样就可以通过读取寄存器，如IRS,来查看当前的中断处理情况。
; RR：
;     Read Register,读取寄存器命令。它和RIS位是配合在一起使用的。当RR为1时才可以读取寄存器。
; RIS:
;     Read Interrupt register Select,读取中断寄存器选择位，
;     用此位选择待读取的寄存器。有点类似显卡寄存器中的索引的意思。
;     若RIS为1，表示选择ISR寄存器，若RIS为0，表示选择RR寄存器。
;     这两个寄存器能否读取，前提是RR的值为1。
;
; ----------------------------------------------------------------------------------------------------------------------
.init_8259a:
    ; ICW1主片
    mov al, 11h ; 指定需发ICW4，级联模式，边沿触发
    out 20h, al
    ; ICW1从片
    out 0A0h, al

    ; ICW2主片
    mov al, 20h ; 主片中断从0x20开始
    out 21h, al
    ; ICW2从片
    mov al, 28h ; 从片中断从0x28开始
    out 0A1h, al

    ; ICW3主片
    mov al, 00000100b ; 第2位为1，即只在IR2引脚发生级联
    out 21h, al
    ; ICW3从片
    mov al, 02h ; 数字2，该从片级联到主片的第2个引脚
    out 0A1h , al

    ; ICW4主片
    mov al, 00000011b ; 自动EOI，x86处理器
    out 21h, al
    ; ICW4从片
    out 0A1h, al

; ----------------------------------------------------------------------------------------------------------------------
; 个人计算机中的级联8259A:
;
;              ----------------      INTR       ----------
;              | Master 8259A |  ------------>  |   CPU  |  <---------- NMI
;              ----------------                 ----------
;              |     IRQ0     | 时钟 (<-低地址)
;              |     IRQ1     | 键盘
;        ----> |     IRQ2     | 连接从片
;        ↑     |     IRQ3     | 串口2
;        |     |     IRQ4     | 串口1
;        |     |     IRQ5     | 并口2
;        |     |     IRQ6     | 软盘
;        |     |     IRQ7     | 并口1 (<-高地址)
;        |     ----------------
;        |
;        |     ----------------
;        <---- | Slave  8259A |
;              ----------------
;              |     IRQ8     | 实时时钟 (<-低地址)
;              |     IRQ9     | 重定向的IRQ2
;              |     IRQ10    | 保留
;              |     IRQ11    | 保留
;              |     IRQ12    | PS/2鼠标
;              |     IRQ13    | FPU异常
;              |     IRQ14    | 硬盘Primary通道
;              |     IRQ15    | 硬盘Secondary通道 (<-高地址)
;              ----------------
; ----------------------------------------------------------------------------------------------------------------------
.init_8259a_OCW1_master:
;    mov al, 11111101b ; 主片开启接收键盘中断
;    mov al, 11111100b ; 主片开启接收键盘和时钟中断
;    mov al, 11111001b ; 主片开启接收键盘中断和级联从片
    mov al, 11111000b ; 主片开启接收键盘中断和时钟中断和级联从片

    out 21h, al

.init_8259a_OCW1_slave:
;    mov al, 11111111b ; 从片屏蔽所有中断
    mov al, 00111111b ; 从片打开硬盘主通道和次通道中断
    out 0A1h, al

.enter_c:
    ; 进入c内核
    call kernel_main

    jmp $