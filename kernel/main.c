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
//    task_init();
    STI

    void* kpage = alloc_page();
    void* upage = alloc_upage();

    free_page(kpage);
    free_upage(upage);


    while (true) {
//        printk("main block! ");
        CLI
        HLT
    }
}
