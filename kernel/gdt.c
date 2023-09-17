//
// Created by toney on 2023-09-02.
//

#include "../include/print.h"
#include "../include/types.h"
#include "../include/dt.h"
#include "../include/string.h"
#include "../include/sys.h"
#include "../include/mm.h"

#define GDT_SIZE 256

#define GDT_REAL_MODE_USED_SIZE 5 // 在实模式下创建的描述符数量，参见 loader.asm 中的定义

#define GDT_CODE_INDEX 1   // 代码段索引值
#define GDT_DATA_INDEX 2   // 数据段索引值
#define GDT_STACK_INDEX 3  // 栈段索引值
#define GDT_SCREEN_INDEX 4 // 屏幕段索引值

#define R3_CODE_GDT_ENTRY_INDEX GDT_REAL_MODE_USED_SIZE       // R3代码段在GDT中的索引值
#define R3_DATA_GDT_ENTRY_INDEX (R3_CODE_GDT_ENTRY_INDEX + 1) // R3数据段段在GDT中的索引值

#define GDT_TOTAL_USED_SIZE (R3_DATA_GDT_ENTRY_INDEX + 1) // 一共创建了多少描述符

uint64 gdt[GDT_SIZE] = {0};
dt_ptr gdt_ptr;

int r3_code_selector;
int r3_data_selector;

// 设置全局描述符表项
// gdt_index: 范围 [GDT_REAL_MODE_USED_SIZE ~ GDT_SIZE)
// type: 内存段类型，只用低四位
// dpl: 描述符特权级，只用低两位
static void set_gdt_entry(int gdt_index, char type, char dpl) {
    global_descriptor *item = &gdt[gdt_index];

    item->base_low = 0;
    item->base_high = 0;        // base = 0
    item->limit_low = 0xffff;
    item->limit_high = 0xf;     // limit = 0xFFFFF, 表示4GB空间

    item->segment = 1;          // 非系统段
    item->type = type & 0xf;    // 取低四位
    item->DPL = dpl & 0b11;    // 取低两位

    item->present = 1;
    item->available = 0;
    item->long_mode = 0;
    item->big = 1;
    item->granularity = 1;
}

/*
 * 修复在实模式下创建的GDT项, 使其符合虚拟地址分页模式:
 *    对于低1M空间, 改前和改后操作回映射到同一块内存地址, 数据不会有任何影响, 只是符合分页规则了
 *    唯一可能由影响的就是对于代码段和数据段, 段界限从4GB改为了1GB
 */
void gdt_virtual_model_fix() {
    // 如果未开启虚拟模式, 则中断此流程
    if (!VIRTUAL_MODEL) {
        return;
    }

    global_descriptor *p;
    for (int i = 0; i < GDT_TOTAL_USED_SIZE; ++i) {
        if (i == GDT_CODE_INDEX) {
            // 数据段与代码段, 基址从 0x0 改为 0xC0000000, 段界限从 4G>>12 改为 1G>>12
            // 注: 内核的实际可用空间应该是1G-4M, 即去除最后一个页表, 这里为了计算方便就不细致处理了, 正常也用不到最后
            p = (global_descriptor *) &gdt[GDT_CODE_INDEX];
            p->base_high += 0xc0;
            p->limit_high >>= 2; // limit原值为0xFFFFF, 只改高位即可
            continue;
        }
        if (i == GDT_DATA_INDEX) {
            p = (global_descriptor *) &gdt[GDT_DATA_INDEX];
            p->base_high += 0xc0;
            p->limit_high >>= 2;
            continue;
        }
        if (i == GDT_STACK_INDEX) {
            // 栈段, 基址从 0x0 改为 0xC0000000, 界限从 0x0 改为 1G>>12
            // 不用管esp，因为栈段中存放的是基址是虚拟地址，对应的真实的物理就是在低1M内存，而esp中存放的是基于ss栈段的真实物理偏移量
            // 或者 ss:esp 组合才是真正的栈顶, 即只要组合值是栈顶值就行, 栈段的虚拟地址在3GB以上, 那实际上栈顶 ss:esp 的虚拟地址也是在3GB以上
            p = (global_descriptor *) &gdt[GDT_STACK_INDEX];
            p->base_high = 0xc0;
            int limit = (1 << 30 >> 12) - 1;
            p->limit_high = limit >> 16;
            p->limit_low = limit;
            continue;
        }
        if (i == GDT_SCREEN_INDEX) {
            // 屏幕段, 基址从 0xb8000 改为 0xc00b8000
            p = (global_descriptor *) &gdt[GDT_SCREEN_INDEX];
            p->base_high = 0xc0;
            continue;
        }
        // 其他段, 修改base和limit即可
        p = (global_descriptor *) &gdt[i];
        p->base_high = 0xC0;        // base = 0xC0000000
        p->limit_high = 0x3;        // limit = 0x3FFFF, 表示3GB空间
    }
}

void gdt_init() {
    printk("init gdt ...\n");

    __asm__ volatile ("sgdt gdt_ptr;"); // 取出GDT
    memcpy(&gdt, (void *) gdt_ptr.base, gdt_ptr.limit);

    gdt_virtual_model_fix();

    // 创建r3用的代码段和数据段描述符
    set_gdt_entry(R3_CODE_GDT_ENTRY_INDEX, 0b1000, 0b11); // 代码段，只执行，用户特权级
    set_gdt_entry(R3_DATA_GDT_ENTRY_INDEX, 0b0010, 0b11); // 数据段，只读，用户特权级

    // 创建r3用的选择子：代码段、数据段
    r3_code_selector = R3_CODE_GDT_ENTRY_INDEX << 3 | 0b011;
    r3_data_selector = R3_DATA_GDT_ENTRY_INDEX << 3 | 0b011;

    gdt_ptr.base = (int) &gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;

    __asm__ volatile ("lgdt gdt_ptr;"); // 重设GDT
}