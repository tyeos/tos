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

    // 虚拟地址到物理地址的映射测试
    // (0x0301 << 22 ) | (0x0004 << 12) | 0x0008
    // PDE index = 0x0301
    // PTE index = 0x0004
    // Offset = 0x0008
    uint *p = (uint *) 0xC0404008;
    // 1, 查找页目录项 0x100000 + 0x0301 * 4 = 0x100C04
    // 1.1, 物理地址 0x100C04 对应值为 0x00102007, 即页目录项的值
    // 2, 查找页表项 0x00102007 & 0xFFFFF000 + 0x0004 * 4 = 0x00102010
    // 2.1, 物理地址 0x00102010 对应值为 0x00107007, 即物理基址
    // 3, 最终物理地址为 0x00107007 & 0xFFFFF000 + 0x0008 = 0x00107008
    *p = 10;

    BOCHS_DEBUG_MAGIC

    while (true);
}
