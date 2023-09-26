# 中断

    中断发生后，eflags中的NT位(Nest Task Flag)和TF位(Trap Flag)会被置0，即非嵌套任务，且禁止单步执行。
    如果是中断门，IF位被自动置0，即不响应可屏蔽中断，避免中断嵌套。
    若是任务门或陷阱门，IF位不会清0，因为陷阱门主要用于调试，允许CPU响应更高级别的中断。
    任务门是执行一个新任务，任务都应该在开中断的情况下进行，以支持多任务。

## 外部中断（硬中断）：

两根中断信号线：

    INTR（可屏蔽中断）：
        Interrupt，如硬盘、打印机、网卡等。
    NMI（不可屏蔽中断）：
        Nno Maskable Interrupt，如电源掉电，内存读写错误、总线奇偶校验错误等。

## 内部中断：

软中断（软件主动发起的中断）：

    int 8位立即数: 可表示256种中断，常通过它进行系统调用。
    
    int3: 断点调试指令，触发中断向量号3。
    into: 中断溢出指令，当eflags的OF位为1时，触发中断向量号4，否则啥也不做。
    bound: 数组索引越界指令，触发中断向量号5。
    ud2: 未定义指令，触发中断向量号6，常用于软件测试，无实际用途。
    
    注：除“int 8位立即数”外，其他四种也叫异常

异常：

    指令执行期间CPU内部产生的错误引起的，不受IF标志位影响（凡是中断关系到“正常”运行，就不受IF位影响）。
    如除零异常（0号中断），无法识别机器码异常（6号中断）等。
    
    按严重程度可分为三种异常：
        (1)Fault, 也称为故障。可被修复，属于最轻的一种异常，如缺页异常 page fault。
        (2)Trap, 也称为陷阱。此异常通常用在调试中，比如int3指令，CPU会将中断处理程序的返回地址指向导致异常指令的下一个指令地址。
        (3)Abort, 也称为终止，最严重的异常类型，一旦出现，由于错误无法修复，程序将无法继续运行，操作系统为了自保，只能将此程序从进程表中去掉。导致此异常的错误通常是硬件错误，或者某些系统数据结构出错。
    
    注：某些异常会有单独的错误码，即error code,进入中断时CPU会把它们压在栈中，这是在压入eip之后做的。

异常与中断：

| Vector No. | Mnemonic | Description                                | Source                                                       | Type       | Error code(Y/N) |
| ---------- | -------- | ------------------------------------------ | ------------------------------------------------------------ | ---------- | --------------- |
| 0          | #DE      | Divide Error                               | DIV and IDIV instructions .                                  | Fault      | N               |
| 1          | #DB      | Debug                                      | Any code or data reference .                                 | Fault/Trap | N               |
| 2          | /        | NMI Interrupt                              | Non-maskable external interrupt.                             | Interrupt  | N               |
| 3          | #BP      | Breakpoint                                 | INT3 instruction.                                            | Trap       | N               |
| 4          | #OF      | Overflow                                   | INTO instruction.                                            | Trap       | N               |
| 5          | #BR      | BOUND Range Exceeded                       | BOUND instruction.                                           | Fault      | N               |
| 6          | #UD      | Invalid Opcode (UnDefined Opcode)          | UD2 instruction or reserved opcode.1                         | Fault      | N               |
| 7          | #NM      | Device Not Available (No Math Coprocessor) | Floating-point or WAIT/FWAIT instruction .                   | Fault      | N               |
| 8          | #DF      | Double Fault                               | Any instruction that can generate an exception , an NMI , or an INTR . | Abort      | Y(0)            |
| 9          | #MF      | CoProcessor Segment Overrun(reserved)      | Floating-point instruction.2                                 | Fault      | N               |
| 10         | #TS      | Invalid TSS                                | Task switch or TSS access.                                   | Fault      | Y               |
| 11         | #NP      | Segment Not Present                        | Loading segment registers or accessing system segments.      | Fault      | Y               |
| 12         | #SS      | Stack Segment Fault                        | Stack operations and SS register loads.                      | Fault      | Y               |
| 13         | #GP      | General Protection                         | Any memory reference and other protection checks.            | Fault      | Y               |
| 14         | #PF      | Page Fault                                 | Any memory reference.                                        | Fault      | Y               |
| 15         |          | Reserved                                   |                                                              |            |                 |
| 16         | #MF      | Floating-Point Error(Math Fault)           | Floating-point or WAIT/FWAIT instruction.                    | Fault      | N               |
| 17         | #AC      | Alignment Check                            | Any data reference in memory.3                               | Fault      | Y(0)            |
| 18         | #MC      | Machine Check                              | Error codes(if any)and source are model dependent.4          | Abort      | N               |
| 19         | #XF      | SIMD Floating-Point Exception              | SIMD Floating-Point Instruction5                             | Fault      | N               |
| 20~31      |          | Reserved                                   |                                                              |            |                 |
| 32~255     |          | Maskable Interrupts                        | External interrupt from INTR pin or INT n instruction.       | Interrupt  |                 |

    Error code: 值为Y则表示相应中断会由CPU压入错误码
    Vector No.: 中断向量号，中断的ID，整数，范围是0~255，用此ID作为中断描述符表中的索引


## 中断表

    中断向量表(Interrupt Vector Table, IVT)：
        实模式下用于存储中断处理程序入口的表
    
    中断描述符表(Interrupt Descriptor Table, IDT)：
        保护模式下用于存储中断处理程序入口的表


## 中断发生时的压栈：

	如果发生特权级转移，使用新特权级栈：
		push SS_old
		push ESP_old
		push EFLAGS
		push CS_old
		push EIP_old
		push ERROR_CODE // 有错误码的异常会压入该值
		
	如果未发生特权级转移，使用当前特权级栈：
		push EFLAGS
		push CS_old
		push EIP_old
		push ERROR_CODE // 有错误码的异常会压入该值
	
	iret指令:
	    interrupt return, 用于从中断程序返回, 其在16位模式下被翻译成iretw, 32位模式下被翻译成iretd。
	    32位模式下从栈顶依次弹出32位数据到EIP、CS、EFLAGS中，如果特权级有变化，会继续依次弹出32位数据到ESP、SS中。
	    如果有中断错误码，需要手动将其跳过，确保iret执行时，栈顶指向EIP_old。

## 中断错误码

    本质上就是个描述符选择子，通过低3位属性来修饰此选择子指向是哪个表中的哪个描述符。
    通常能够压入错误码的中断属于中断向量号在0~32之内的异常，
    而外部中断（中断向量号在32~255之间)和int软中断并不会产生错误码。通常我们并不用处理错误码。

结构：

    -------------------------------------------------------------------------------------------------
    | 31  | 30  | 29  | 28  | 27  | 26  | 25  | 24  | 23  | 22  | 21  | 20  | 19  | 18  | 17  | 16  |
    |                                       保留位，全为0                                             |
    -------------------------------------------------------------------------------------------------
    -------------------------------------------------------------------------------------------------
    | 15  | 14  | 13  | 12  | 11  | 10  |  9  |  8  |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
    |                               选择子高13位索引                                |  TI | IDT | EXT |
    -------------------------------------------------------------------------------------------------
    EXT:
        external event, 即外部事件，用来指明中断源是否来自处理器外部，
        如果中断源是不可屏蔽中断NMI或外部设备EXT为1，否则为0。
    DT:
        表示选择子是否指向中断描述符表DT, DT位为1表示此选择子指向中断描述符表，
        否则指向全局描述符表GDT或局部描述符表LDT。
    TI:
        和选择子中TI是一个意思，为0时用来指明选择子是从GDT中检索描述符，为1时是从LDT中检索描述符。
        只有在DT位为0时TI位才有意义。
    选择子高13位索引:
        选择子中用来在表中索引描述符用的下标。


## 中断代理芯片 Intel 8259A

    可编程中断控制器（PCI）8259A，作用是负责所有来自外设的中断，其中就包括来自时钟的中断。
    8259A用于管理和控制可屏蔽中断，它表现在屏蔽外设中断，对它们实行优先级判决，向CPU提供中断向量号等功能。
    而它称为可编程的原因，就是可以通过编程的方式来设置以上的功能。

级联(cascade)：

    8259A只可以管理8个中断，为了多支持一些中断设备，提供了另一个解决方案，将多个8259A组合，官方术语就是级联。
    有了级联这种组合后，每一个8259A就被称为1片。若采用级联方式，即多片8259A芯片串连在一起，最多可级联9个，也就是最多支持64个中断。
    n片8259A通过级联可支持7n+1个中断源，级联时只能有一片8259A为主片master,其余的均为从片slave。
    来自从片的中断只能传递给主片，再由主片向上传递给CPU,也就是说只有主片才会向CPU发送INT中断信号。

IRQ:

    Interrupt Request, 单个8259A芯片只有8个中断请求信号线：IRQ0~IRQ7.
    个人电脑中只有两片8259A芯片，一共16个IRQ接口, 级联一个从片要占用主片一个IRQ接口,所以最多支持15个中断。


## 8259A芯片内部结构 （内部寄存器宽度都是8位）

    IMR:
        Interrupt Mask Register, 中断屏蔽寄存器，用来屏蔽某个外设的中断。
        外设发送的中断都要先经过这里。
    IRR:
        Interrupt Request Register, 中断请求寄存器，用于接收并锁存经过IMR寄存器过滤后的中断信号，
        此寄存器中全是等待处理的中断，“相当于”8259A维护的未处理中断信号队列。
    PR:
        Priority Resolver, 优先级仲裁器。
        当有多个中断同时发生，或当有新的中断请求进来时，将它与当前正在处理的中断进行比较，找出优先级更高的中断。
    控制电路：
        INT:
            中断信号，用于通知CPU中断, 将信号发送到CPU的INTR接口。
        INTA:
            INT Acknowledge, 中断响应信号, 用于接收来自CPU的INTA接口的中断响应信号。
    ISR:
        In-Service Register, 中断服务寄存器。当某个中断正在被处理时，保存在此寄存器中。

## 8259A的寄存器组:

    初始化命令寄存器组：
        用于初始化命令字（Initialization Command Words, ICW）,ICW共4个，ICW1~ICW4，须按顺序写入。
    操作命令寄存器组：
        用来保存操作命令字(Operation Command Word, OCW), OCW共3个，OCW1~OCW3，发送顺序不要求。





## INT 10H 中断

> 参考1：
> [维基百科: INT 10H](https://zh.wikipedia.org/zh-hans/INT_10H)
>
> 参考2：
> [CSDN: INT 0x10](https://zh.wikipedia.org/zh-hans/INT_10H)


---

# 