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

