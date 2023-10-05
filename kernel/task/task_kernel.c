//
// Created by toney on 9/30/23.
//

#include "../../include/task.h"
#include "../../include/print.h"
#include "../../include/sys.h"
#include "../../include/lock.h"
#include "../../include/ide.h"
#include "../../include/string.h"


/* 模拟内核任务A */
void *kernel_task_a(void *args) {
    for (int i = 0; i < 10; ++i) {
        printk("K_A ~ %d\n", i);
        HLT
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

/*
 * 处理硬盘初始化
 */
void *kernel_task_ide(void *args) {
    printk("K_IDE start~\n");
    ide_init();

//    int32 fd = sys_open("/file1", O_CREAT);
    int32 fd = sys_open("/file1", O_RDWR);
    printk("K_IDE open fd = %d\n", fd);

//    int32 cnt = sys_write(fd, "hello world ~\n", 13);
//    printk("K_IDE write cnt = %d\n", cnt);

    char buf[64] = {0};

    int32 cnt = sys_read(fd, buf, 8);
    printk("K_IDE read 1 [%d]: %s\n", cnt, buf);

    memset(buf, 0, 64);
    cnt = sys_read(fd, buf, 8);
    printk("K_IDE read 2 [%d]: %s\n", cnt, buf);

    int32 seek = sys_seek(fd, 1, SEEK_SET);
    printk("K_IDE seek = %d\n", seek);

    memset(buf, 0, 64);
    cnt = sys_read(fd, buf, 8);
    printk("K_IDE read 3 [%d]: %s\n", cnt, buf);


    int32 ret = sys_close(fd);
    printk("K_IDE close ret = %d\n", ret);

//    const uint32 page_count = 1;
//    uint32 *pages[page_count];
//    for (int i = 0; i < page_count; ++i) {
//        pages[i] = alloc_kernel_page();
//        if (i > 0 && (uint32) pages[i] != (uint32) pages[i - 1] + PAGE_SIZE) {
//            printk("Discontinuous address !");
//            STOP
//        }
//    }
//    int sec_cnt = page_count * 8; // 一页8个扇区
//    char *buff = (char *) pages[0];
//
//    // 测试读盘
//    printk("test ide read start ~ \n");
//    ide_read(get_disk(0, 0), 0, buff, sec_cnt);
//    print_hex_buff(buff, 1 << 9);
//    printk(".. to 0x%x:\n", ((sec_cnt - 1) << 9));
//    print_hex_buff(buff + ((sec_cnt - 1) << 9), 1 << 9);
//    printk("test ide read end ~ \n");
//
//    // 测试写盘
//    printk("test ide write start ~~ \n");
//    ide_write(get_disk(1, 0), 1, buff, sec_cnt);
//    printk("test ide write end ~~ \n");
//
//    for (int i = 0; i < page_count; ++i) {
//        free_kernel_page(pages[i]);
//    }
    printk("K_IDE end~\n");
    SLEEP(100)
    printk("K_IDE exit~\n");
    return NULL;
}

