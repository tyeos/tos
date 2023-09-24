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

void kernel_main(void) {
    console_clear();
    memory_init();
    gdt_init();
    idt_init();
    clock_init();
    syscall_init();
//    task_init();
    STI

    void* kpage1 = alloc_kernel_page();
    void* kpage2 = alloc_kernel_page();
    void* upage1 = alloc_user_page();
    void* upage2 = alloc_user_page();

    free_kernel_page(kpage1);
    free_kernel_page(kpage2);
    free_user_page(upage1);
    free_user_page(upage2);

    void* kpage3 = alloc_kernel_page();
    void* upage3 = alloc_user_page();
    free_user_page(upage3);
    free_kernel_page(kpage3);


    while (true) {
//        printk("main block! ");
        HLT
    }
}
