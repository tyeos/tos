//
// Created by toney on 2023-09-02.
//

#include "../include/linux/kernel.h"
#include "../include/linux/types.h"
#include "../include/dt.h"
#include "../include/string.h"
#include "../include/bridge/sys.h"

#define GDT_SIZE 256
#define GDT_REAL_MODE_USED_SIZE 15 // 在实模式下用掉了一部分

#define R3_CODE_GDT_ENTRY_INDEX GDT_REAL_MODE_USED_SIZE     // R3代码段在GDT中的索引值
#define R3_DATA_GDT_ENTRY_INDEX (R3_CODE_GDT_ENTRY_INDEX + 1) // R3数据段段在GDT中的索引值

uint64 gdt[GDT_SIZE] = {0};
dt_ptr gdt_ptr;

int r3_code_selector;
int r3_data_selector;

// 设置全局描述符表项
// gdt_index: 范围 [GDT_REAL_MODE_USED_SIZE ~ GDT_SIZE)
// type: 只用低四位
static void set_gdt_entry(int gdt_index, char type) {
    global_descriptor *item = &gdt[gdt_index];

    item->base_low = 0;
    item->base_high = 0;        // base = 0
    item->limit_low = 0xffff;
    item->limit_high = 0xf;     // limit = 0xfffff

    item->segment = 1;          // 非系统段
    item->type = type & 0xf;    // 取低四位

    item->DPL = 0b11;
    item->present = 1;
    item->available = 0;
    item->long_mode = 0;
    item->big = 1;
    item->granularity = 1;
}

void gdt_init() {
    printk("init gdt ...\n");

    __asm__ volatile ("sgdt gdt_ptr;"); // 取出GDT
    memcpy(&gdt, (void *) gdt_ptr.base, gdt_ptr.limit);

    // 创建r3用的代码段和数据段描述符
    set_gdt_entry(R3_CODE_GDT_ENTRY_INDEX, 0b1000); // 代码段，只执行
    set_gdt_entry(R3_DATA_GDT_ENTRY_INDEX, 0b0010); // 数据段，只读

    gdt_ptr.base = (int) &gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;

    BOCHS_DEBUG_MAGIC
    __asm__ volatile ("lgdt gdt_ptr;"); // 重设GDT
}