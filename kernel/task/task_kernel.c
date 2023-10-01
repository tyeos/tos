//
// Created by toney on 9/30/23.
//

#include "../../include/task.h"
#include "../../include/print.h"
#include "../../include/sys.h"
#include "../../include/lock.h"
#include "../../include/ide.h"


/* 模拟内核任务A */
void *kernel_task_a(void *args) {
    printk("K_A start~\n");

    const uint32 page_count = 1;
    uint32 *pages[page_count];
    for (int i = 0; i < page_count; ++i) {
        pages[i] = alloc_kernel_page();
        if (i > 0 && (uint32) pages[i] != (uint32) pages[i - 1] + PAGE_SIZE) {
            printk("Discontinuous address !");
            STOP
        }
    }
    int sec_cnt = page_count * 8; // 一页8个扇区
    char *buff = (char *) pages[0];

    // 测试读盘
    printk("test ide read start ~ \n");
    ide_read(get_disk(0, 0), 0, buff, sec_cnt);
    print_hex_buff(buff, 1 << 9);
    printk(".. to 0x%x:\n", ((sec_cnt - 1) << 9));
    print_hex_buff(buff + ((sec_cnt - 1) << 9), 1 << 9);
    printk("test ide read end ~ \n");

    // 测试写盘
    printk("test ide write start ~~ \n");
    ide_write(get_disk(1, 0), 1, buff, sec_cnt);
    printk("test ide write end ~~ \n");

    for (int i = 0; i < 10; ++i) {
        printk("K_A ~ %d\n", i);
        HLT
    }
    printk("K_A end~\n");
//    SLEEP(100)
    for (int i = 0; i < page_count; ++i) {
        free_kernel_page(pages[i]);
    }

    return NULL;
}

/* 模拟内核任务B */
void *kernel_task_b(void *args) {
    for (int i = 0; i < 10; ++i) {
        printk("K_B ~~~ %d\n", i);
        HLT
    }
    return NULL;
}
