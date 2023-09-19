//
// Created by toney on 23-8-26.
//

#include "../include/console.h"
#include "../include/print.h"
#include "../include/init.h"
#include "../include/sys.h"
#include "../include/mm.h"
#include "../include/task.h"

// 测试切换到用户态
extern void move_to_user() ;

void user_entry() {
    BOCHS_DEBUG_MAGIC
    BOCHS_DEBUG_MAGIC
    int i = 10;
}

void *kernel_thread(void *args) {
    move_to_user();
    while (true) {
        asm volatile("sti; hlt;");
    }
}


void kernel_main(void) {
    console_clear();
    memory_init();
    virtual_memory_init(); // 开启分页, 使用虚拟内存地址
    gdt_init();
    idt_init();
    clock_init();
    task_init();
    STI

//    kernel_thread(0);

    while (true) {
        printk("main block! ");
        HLT
    }
}
