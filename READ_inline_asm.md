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

# 