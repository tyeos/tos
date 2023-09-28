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
#include "../include/bridge/io.h"


#define port_read(port, buf, nr) \
__asm__("cld;rep;insw"::"d"(port),"D"(buf),"c"(nr))

#define port_write(port, buf, nr) \
__asm__("cld;rep;outsw"::"d"(port),"S"(buf),"c"(nr))

char buff[512] = {0};

void hd_handler() {
    port_read(0x1f0, buff, 512);
    for (int i = 0; i < 512; ++i) {
        if (i % 16 == 0) printk("\n");
        printk("%02x ", buff[i] & 0xff);
    }

    printk("\nhd_handler end~ \n");

}


void kernel_main(void) {
    console_clear();
    physical_memory_init();
    virtual_memory_init();
    gdt_init();
    idt_init();
    clock_init();
    syscall_init();
    task_init();
    STI

//    uint8 hd_cnt = *((uint8 *) (0x475));    // 获取硬盘的数量
//    printk("hd_cnt = %d! \n", hd_cnt);
//    BOCHS_DEBUG_MAGIC
//
//    char hd = 0;
//    int from = 0;
//    int count = 1;
//    int cmd = 0x20; // 读扇区
//
//    // 主盘 0x1f0
//    outb(0x1f0, count);
//    outb(0x1f3, from);
//    outb(0x1f4, from >> 8);
//    outb(0x1f5, from >> 16);
//    outb(0x1f6, 0b11100000 | (hd << 4) | (from >> 24 & 0xf));
//    outb(0x1f7, cmd);


    while (true) {
        printk("main block! \n");
        SLEEP(5)
    }
}
