# tos 简介

一款微型操作系统

## 目前已实现：

> 通过MBR载入内核
>
> bochs 汇编调试环境
>
> 从实模式进入保护模式
>
> 进入C编写内核
>
> qmue C调试环境
>
> 操控显卡向屏幕输出字符


> 通过8259A接收外部中断
>
> 键盘驱动识别输入字符（键盘中断）
>
> Intel 8253 时钟中断
>
> 窗口多屏缓存与滚屏控制
>
> 实现可变参数与printk

> 内存检测
>
> 开启(101012)分页模式（虚拟地址管理）
>
> 内存分配与释放（物理页、虚拟页与内存池、位图管理）与页表挂载（动态页表自动伸缩）
>
> 分页模式下的调试环境

> 双向链表
>
> r0内核态与r3用户态的切换（实现int0x80系统调用）
>
> 多进程的虚拟页目录及页表管理（多用户进程分配独立虚拟页目录表，共用高1GB内核PDE）
>
> 多任务调度（中断控制，进程与线程、内核态与用户态切换，用户态资源回收）


## 进行中：


> 互斥锁
> 
> 硬盘驱动
>
> 文件管理系统
>
> shell命令窗口


---

# 关于汇编指令：

## [NSAM (The Netwide Assembler)](https://www.nasm.us/doc/)

> 部分关键指令：
>
>> [ORG](https://www.nasm.us/doc/nasmdoc8.html#section-8.1.1)
>>
>> [SECTION](https://www.nasm.us/doc/nasmdoc7.html#section-7.3)
>>
>> [BITS](https://www.nasm.us/doc/nasmdoc7.html#section-7.1)
>>
>> [EQU](https://www.nasm.us/doc/nasmdoc3.html#section-3.2.4)

---

# 实模式下的1MB内存布局

| 起始    | 结束    | 大小            | 用途                                                                                 |
|-------|-------|---------------|------------------------------------------------------------------------------------|
| FFFF0 | FFFFF | 16B           | BIOS入口地址，此地址也属于BIOS代码，同样属于顶部的640KB字节。只是为了强调其入口地址才单独贴出来。此处16字节的内容是跳转指令jmp f000:e05b |
| F0000 | FFFEF | 64KB-16B      | 系统BIOS范围是F0000~FFFFF共640KB,为说明入口地址，将最上面的16字节从此处去掉了，所以此处终止地址是OXFFFEF                |
| C8000 | EFFFF | 160KB         | 映射硬件适配器的ROM或内存映射式I/O                                                               |
| C0000 | C7FFF | 32KB          | 显示适配器BIOS                                                                          |
| B8000 | BFFFF | 32KB          | 用于文本模式显示适配器                                                                        |
| B0000 | B7FFF | 32KB          | 用于黑白显示适配器                                                                          |
| A0000 | AFFFF | 64KB          | 用于彩色显示适配器                                                                          |
| 9FC00 | 9FFFF | IKB           | EBDA(Extended BIOS Data Area)扩展BIOS数据区                                             |
| 7E00  | 9FBFF | 622080B约608KB | 可用区域                                                                               |
| 7C00  | 7DFF  | 512B          | MBR被BIOS加载到此处，共512字节                                                               |
| 500   | 7BFF  | 30464B约30KB   | 可用区域                                                                               |
| 400   | 4FF   | 256B          | BIOS Data Area(BIOS数据区）                                                            |
| 000   | 3FF   | 1KB           | Interrupt Vector Table( IVT, 中断向量表）                                                |

---

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

| Vector No. | Mnemonic | Description                                | Source                                                                 | Type       | Error code(Y/N) |
|------------|----------|--------------------------------------------|------------------------------------------------------------------------|------------|-----------------|
| 0          | #DE      | Divide Error                               | DIV and IDIV instructions .                                            | Fault      | N               | 
| 1          | #DB      | Debug                                      | Any code or data reference .                                           | Fault/Trap | N               |
| 2          | /        | NMI Interrupt                              | Non-maskable external interrupt.                                       | Interrupt  | N               |
| 3          | #BP      | Breakpoint                                 | INT3 instruction.                                                      | Trap       | N               |
| 4          | #OF      | Overflow                                   | INTO instruction.                                                      | Trap       | N               |
| 5          | #BR      | BOUND Range Exceeded                       | BOUND instruction.                                                     | Fault      | N               |
| 6          | #UD      | Invalid Opcode (UnDefined Opcode)          | UD2 instruction or reserved opcode.1                                   | Fault      | N               |
| 7          | #NM      | Device Not Available (No Math Coprocessor) | Floating-point or WAIT/FWAIT instruction .                             | Fault      | N               |
| 8          | #DF      | Double Fault                               | Any instruction that can generate an exception , an NMI , or an INTR . | Abort      | Y(0)            |
| 9          | #MF      | CoProcessor Segment Overrun(reserved)      | Floating-point instruction.2                                           | Fault      | N               |
| 10         | #TS      | Invalid TSS                                | Task switch or TSS access.                                             | Fault      | Y               |
| 11         | #NP      | Segment Not Present                        | Loading segment registers or accessing system segments.                | Fault      | Y               |
| 12         | #SS      | Stack Segment Fault                        | Stack operations and SS register loads.                                | Fault      | Y               |  
| 13         | #GP      | General Protection                         | Any memory reference and other protection checks.                      | Fault      | Y               |  
| 14         | #PF      | Page Fault                                 | Any memory reference.                                                  | Fault      | Y               |  
| 15         |          | Reserved                                   |                                                                        |            |                 |  
| 16         | #MF      | Floating-Point Error(Math Fault)           | Floating-point or WAIT/FWAIT instruction.                              | Fault      | N               |  
| 17         | #AC      | Alignment Check                            | Any data reference in memory.3                                         | Fault      | Y(0)            |  
| 18         | #MC      | Machine Check                              | Error codes(if any)and source are model dependent.4                    | Abort      | N               |  
| 19         | #XF      | SIMD Floating-Point Exception              | SIMD Floating-Point Instruction5                                       | Fault      | N               |  
| 20~31      |          | Reserved                                   |                                                                        |            |                 |  
| 32~255     |          | Maskable Interrupts                        | External interrupt from INTR pin or INT n instruction.                 | Interrupt  |                 |  

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

# 内联汇编

内联汇编指令：asm

    C语言中的内联汇编指令，有三种风格：asm 或 __asm 或 __asm__
    内联汇编采用AT&T语法，操作对象和Intel是反着的。

## 基本内联汇编格式：

> asm [volatile] ("assembly code")

    asm("pusha;" "mov $4, %eax; \
        popa")

    多条指令间用分号隔开，最后一行指令后的分号加不加都行

## 扩展内联汇编格式：

> asm [volatile] ("assembly code" : output : input : clobber/modify)

### 参数说明：

assembly code 中的变量规则:

    百分号'%'在扩展内联汇编中作为转义字符。

    $number: 立即数
        如：$6

    %%register: 寄存器
        如：%%eax

    %[opcode]index: 表示第几个参数（按output、input参数顺序排列，index范围0~9）
        opcode（省略会自动判断走32位还是低16位或低8位）:
        b:输出寄存器中低部分1字节对应的名称，如al、bl、cl、dl。
        h:输出寄存器高位部分中的那一字节对应的寄存器名称，如ah、bh、ch、dh。
        w:输出寄存器中大小为2个字节对应的部分，如ax、bx、cx、dx。
        k:输出寄存器的四字节部分，如eax、ebx、ecx、edx。
        ... 其他操作码参考i386.c官方源码，如gcc-4.9.0/gcc/config/i386/i386.c第14741~14772行
        如：
            %1: 表示第1个参数（output参数为=a时，%1则表示eax）
            %b0: 表示第0个参数的低八位（output参数为=a时，%b0则表示al）

    %[name]: 使用名称占位符，名称在output、input中显示定义
        如：asm("mov %[from], %%eax;":"=a"(out):[from]"b"(in)); 表示把变量in中的值赋给out变量

output格式（多个操作数间用逗号分隔）:

    "操作修饰符约束名"(C变量名)

    output的操作修饰符：
        =:表示操作数是只写，相当于为output括号中的C变量赋值，如=a(out),此修饰符相当于out=eax。
        +:表示操作数是先读后写，告诉gcc所约束的寄存器或内存先被读入，最后再被写出。
        &:表示此output中的操作数要独占所约束（分配）的寄存器，只供output使用，任何input中所分配的寄存器不能与此相同。当表达式中有多个修饰符时，&要与约束名挨着，不能分隔。

    注：一般情况下用等号，即只写

input格式（多个操作数间用逗号分隔）:

    "[操作修饰符]约束名"(C变量名)

    input的操作修饰符：
        %:该操作数可以和下一个输入操作数互换。

    注：一般情况下省略，即只读

output和input中的约束：

    寄存器约束：
        a:表示寄存器eax/ax/al
        b:表示寄存器ebx/bx/bl
        c:表示寄存器ecx/cx/cl
        d:表示寄存器edx/dx/dl
        D:表示寄存器edi/di
        S:表示寄存器esi/si
        q:表示任意这4个通用寄存器之一：eax/ebx/ecx/edx
        r:表示任意这6个通用寄存器之一：eax/ebx/ecx/edx/esi/edi
        g:表示可以存放到任意地点（寄存器和内存）
        A:把eax和edx组合成64位整数
        f:表示浮点寄存器
        t:表示第1个浮点寄存器
        u:表示第2个浮点寄存器

    内存约束：
        m:表示操作数可以使用任意一种内存形式。
        o:操作数为内存变量，但访问它是通过偏移量的形式访问，即包含offset_address的格式。

    立即数约束：
        I:表示操作数为整数立即数
        F:表示操作数为浮点数立即数
        I:表示操作数为0~31之间的立即数
        J:表示操作数为0~63之间的立即数
        N:表示操作数为0~255之间的立即数
        O:表示操作数为0~32之间的立即数
        X:表示操作数为任何类型立即数

    通用约束：
        0~9:此约束只用在input部分，但表示可与output和input中第n(0~9)个操作数用相同的寄存器或内存
    
    如: 
        int i = 1, sum = 0; 
        __asm("add %1, %0;":"=a"(sum):"%I"(2),"0"(i)); 
    解释为：
        输入 "%I"(2) 为立即数 2
        输入 "0"(i) 中的 "0" 相当于 "=a", 即输入 eax=i
        汇编指令 "add %1, %0;" 相当于 eax=2+eax, 即 eax=2+i
        输出 "=a"(sum) 相当于 sum=eax, 即 sum=2+i

clobber/modify:

    用于通知gcc修改了哪些寄存器或内存。

        C代码本身翻译也要用到寄存器，在C程序中嵌入了汇编代码，必然会造成一些资源的破坏，会使gcc重新为C代码安排寄存器等资源，
        为了解决资源冲突，我们得让gcc知道我们改变了哪些寄存器或内存，这样gcc才能合理安排。
        如果在output和input中通过寄存器约束指定了寄存器，gcc必然会知道这些寄存器会被修改，
        所以需要在clobber/modify中通知的寄存器是没有在output和input中出现过的。

    如：
        asm("mov %%eax,%%ebx":"=m"(out)::"bx")
        注：bx用bl或ebx都行，只要能让gcc能区分是谁就行。

---

# ABI
    先说API, API是Application Programming Interface, 即应用程序可编程接口，库函数和操作系统的系统调用之间的接口。

    ABI是Application Binary Interface, 即应用程序二进制接口，ABI与API不同，ABI规定的是更加底层的一套规则，
    属于编译方面的约定，比如参数如何传递，返回值如何存储，系统调用的实现方式，目标文件格式或数据类型等。
    
    在X86架构里面，有一份官方规范SysV_ABI_386-v4文档，有一段原文:
    All registers on the Intel386 are global and thus visible to both a calling and a called function.
    Registers %ebp, %ebx, %edi, %esi, and %esp “ belong ” to the calling function. In other words,
    a called function must preserve these registers' values for its caller.
    Remaining registers “ belong ” to the called function.  
    If  a calling function wants to  preserve such a register value across a function call,
    it must save the value in its local stack frame.
    
    大体意思是在i386体系架构上，函数调用时，这5个寄存器ebp、ebx、edi、esi、esp归主调函数所有，其余寄存器归被调函数所用。
    也就是说，不管被调函数中是否使用了这5个寄存器，在被调函数执行后，这5个寄存器的值不变。
    也就是被调函数至少要为主调函数保护好这5个寄存器的值，至于其他值不做保证。

---
