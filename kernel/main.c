//
// Created by toney on 23-8-26.
//

#include "../include/console.h"
#include "../include/linux/kernel.h"

void kernel_main(void) {
    console_clear();

    for (int i = 0; i < 110; ++i) {
        printk("i = %3d\n", i);
    }

    int a = 0x12345F;
    printk("addr [0x%p] value is [0x%08x]", &a, a);
}
