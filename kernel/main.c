//
// Created by toney on 23-8-26.
//

#include "../include/console.h"
#include "../include/print.h"
#include "../include/init.h"
#include "../include/sys.h"
#include "../include/mm.h"
#include "../include/task.h"
#include "../include/syscall.h"

void kernel_main(void) {
    console_clear();
    memory_init();
    virtual_memory_init(); // 开启分页, 使用虚拟内存地址
    gdt_init();
    idt_init();
    clock_init();
    syscall_init();
    task_init();
    STI

    BOCHS_DEBUG_MAGIC
    BOCHS_DEBUG_MAGIC
    
    while (true) {
        printk("main block! ");
        HLT
    }
}
