//
// Created by toney on 23-9-20.
//

#include "../include/print.h"


void ksyscall_no1(int i){
    printk("syscall_no1: 0x%x\n", i);
}

void* syscall_table[] = {printk, ksyscall_no1};