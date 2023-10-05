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

    char *test_dir = "/test1";
    char *test_file = "/test1/file1";

    int32 mkdir = sys_mkdir(test_dir);
    printk("K_IDE mkdir = %d\n", mkdir);
    if (mkdir == -1) STOP

    dir_t *opendir = sys_opendir(test_dir);
    printk("K_IDE opendir = %d\n", opendir);
    if (!opendir) STOP

    int32 fd = sys_open(test_file, O_CREAT | O_RDWR);
    printk("K_IDE open fd = %d\n", fd);
    if (fd == -1) STOP

    int32 write = sys_write(fd, "hello world ~\n", 13);
    printk("K_IDE write = %d\n", write);

    char buf[64] = {0};
    int32 read = sys_read(fd, buf, 8);
    printk("K_IDE read [%d]: %s\n", read, buf);

    dir_entry_t *readdir = sys_readdir(opendir);
    printk("K_IDE readdir: %s\n", readdir->name);

    int32 close = sys_close(fd);
    printk("K_IDE close = %d\n", close);

    int32 unlink = sys_unlink(test_file);
    printk("K_IDE unlink = %d\n", unlink);

    int32 closedir = sys_closedir(opendir);
    printk("K_IDE closedir = %d\n", closedir);

    int32 rmdir = sys_rmdir(test_dir);
    printk("K_IDE rmdir = %d\n", rmdir);

    printk("K_IDE end~\n");
    SLEEP(100)
    printk("K_IDE exit~\n");
    return NULL;
}

