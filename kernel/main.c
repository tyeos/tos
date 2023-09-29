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
#include "../include/io.h"

void kernel_main(void) {
    console_clear();
    physical_memory_init();
    virtual_memory_init();
    gdt_init();
    idt_init();
    clock_init();
    syscall_init();
    task_init();
    ide_init();
    STI

    while (true) {
        printk("main block! \n");
        SLEEP(5)
    }
}
