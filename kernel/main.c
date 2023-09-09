//
// Created by toney on 23-8-26.
//

#include "../include/console.h"
#include "../include/print.h"
#include "../include/init.h"
#include "../include/bridge/sys.h"
#include "../include/mm.h"

void kernel_main(void) {
    console_clear();
    memory_init();
    virtual_memory_init(); // 开启分页, 使用虚拟内存地址

    gdt_init();
    idt_init();
    clock_init();
    printk("kernel inited!\n");
    STI

    while (true);
}
