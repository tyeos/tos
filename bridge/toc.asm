; 该文件使用elf格式编译，所以不用加ORG声明
[SECTION .text]
[BITS 32]

extern kernel_main

global _start
_start:
    call kernel_main

    jmp $