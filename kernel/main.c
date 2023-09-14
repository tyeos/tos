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


    // 按页申请内存
    void* p1 = alloc_page();
    printk("malloc p1 = 0x%p\n", p1);
    void* p2 = alloc_page();
    printk("malloc p2 = 0x%p\n", p2);
    void* p3 = alloc_page();
    printk("malloc p3 = 0x%p\n", p3);
    *(int*)p1 = 10;
    *(int*)p2 = 11;
    *(int*)p3 = 12;
    BOCHS_DEBUG_MAGIC

    free_page(p1);
    free_page(p3);
    free_page(p2);

    BOCHS_DEBUG_MAGIC

    // 按字节申请内存
    p1 = malloc(1);
    p2 = malloc(100);
    p3 = malloc(3);
    printk("malloc p1 = 0x%p\n", p1);
    printk("malloc p2 = 0x%p\n", p2);
    printk("malloc p3 = 0x%p\n", p3);

    BOCHS_DEBUG_MAGIC

    free_s(p1, 1);
    free(p2);
    free(p3);

    BOCHS_DEBUG_MAGIC
    BOCHS_DEBUG_MAGIC

    while (true);
}
