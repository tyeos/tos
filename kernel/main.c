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


    int pde_index = 0x305;
    int pte_index = 2;
    int offset = 16;
    int *vp = (int *) ((pde_index << 22) | (pte_index << 12) | offset);
    check_create_virtual_page(pde_index,pte_index);
    printk("vp = 0x%p\n", vp);
    *vp = 10;


//    int* a = ( int* )get_virtual_addr();
//    *a = 10;
//    printk("malloc a = 0x%p\n", *a);
//    void* p1 = malloc(1);
//    void* p2 = malloc(100);
//    void* p3 = malloc(3);
//    printk("malloc p1 = 0x%p\n", p1);
//    printk("malloc p2 = 0x%p\n", p2);
//    printk("malloc p3 = 0x%p\n", p3);
//
//    free_s(p1, 1);
//    free(p2);
//    free(p3);


    BOCHS_DEBUG_MAGIC
    BOCHS_DEBUG_MAGIC
    BOCHS_DEBUG_MAGIC

    while (true);
}
