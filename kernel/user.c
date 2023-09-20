//
// Created by toney on 23-9-20.
//


#include "../include/task.h"


void printf(char *msg) {
    asm volatile("mov eax, 0; int 0x80;"::"b"(msg)); // 发起0号中断调用
}

void syscall_no1(int i) {
    asm volatile("mov eax, 1; int 0x80;"::"b"(i)); // 发起1号中断调用
}

// 从汇编中回调
void user_entry() {
    int i = 10;
    printf("user print 1\n");
    syscall_no1(0x88);

    while (true);
}

