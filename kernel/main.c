//
// Created by toney on 23-8-26.
//

#include "../include/console.h"
#include "../include/linux/kernel.h"
#include "../include/init.h"
#include "../include/bridge/sys.h"

void kernel_main(void) {
    console_clear();
    gdt_init();
    idt_init();
    printk("kernel inited!\n");
    STI

    while (true);
}
