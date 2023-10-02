//
// Created by toney on 2023-09-08.
//

#include "../../include/mm.h"
#include "../../include/print.h"
#include "../../include/sys.h"

#define ARDS_TIMES_ADDR 0xE00
#define ARDS_BUFFER_ADDR 0x1000

/*
 * 1MB以上为有效使用内存, 1MB+8KB以上作为有效分配内存
 * 这8KB内存用于存放页目录表及第0个页表
 */
#define AVAILABLE_START_MEMORY_FROM 0x100000
#define AVAILABLE_ALLOC_MEMORY_FROM 0x102000

/*
 * 物理内存按页向外提供
 */
#define PHYSICAL_MEM_BITMAP_ADDR 0x10000     // 内存分配对应的位图存储起始地址
memory_alloc_t g_physical_memory;

void physical_memory_init() {
    checked_memory_info_t info;
    info.times = *(uint16 *) ARDS_TIMES_ADDR;
    info.ards = (ards_t *) ARDS_BUFFER_ADDR;

    bool ok = false;
    for (uint32 from, to, len, i = 0; i < info.times; i++) {
        ards_t *p = info.ards + i;

        // uint32可以表示4GB空间，在101012模式下，用低位就足够，高位一定为0
        from = p->base_addr_low;
        len = p->length_low;
        to = p->base_addr_low + p->length_low - 1;
        printk("memory: addr=[0x%x ~ 0x%x], len=0x%x, type=%d\n", from, to, len, p->type);

        /*
         * 正常情况下, 从0x100000开始的一段内存一定可用.
         * linux下虚拟环境实测有两块内存可用:
         *    第一块从0x0(qemu、bochs)开始, 截至在0x9fbff(qemu) 或 0x9efff(bochs)
         *    第二块从0x100000(qemu、bochs)开始, 截至在0x1efffff(qemu) 或 0x1fdffff(bochs)
         */
        if (!ok && from == AVAILABLE_START_MEMORY_FROM && to > AVAILABLE_ALLOC_MEMORY_FROM && p->type == 1) {
            ok = true;

            // 物理内存
            g_physical_memory.addr_start = AVAILABLE_ALLOC_MEMORY_FROM;
            g_physical_memory.addr_end = to;
            g_physical_memory.available_size = g_physical_memory.addr_end - g_physical_memory.addr_start + 1;
            g_physical_memory.bitmap.total = g_physical_memory.available_size >> 12; // 页大小4K
            g_physical_memory.bitmap.map = (uint8 *) PHYSICAL_MEM_BITMAP_ADDR;
            bitmap_init(&g_physical_memory.bitmap);
        }
    }

    if (!ok) {
        printk("[%s:%d] unavailable physical memory!\n", __FILE__, __LINE__);
        return;
    }

    printk("available physical memory pages %d: [0x%x~0x%x]\n", g_physical_memory.bitmap.total,
           g_physical_memory.addr_start, g_physical_memory.addr_end);
}

// 分配可用物理页
void *alloc_physical_page() {
    if (g_physical_memory.addr_start != AVAILABLE_ALLOC_MEMORY_FROM) {
        printk("[%s] no memory available!\n", __FUNCTION__);
        return NULL;
    }

    uint32 idx = bitmap_alloc(&g_physical_memory.bitmap);
    void *p = (void *) (g_physical_memory.addr_start + (idx << 12));
    if (OPEN_MEMORY_LOG)
        printk("[%s] alloc page: 0x%X, used: %d pages\n", __FUNCTION__, p, g_physical_memory.bitmap.total);
    return p;
}

// 释放物理页
void free_physical_page(void *p) {
    if ((uint) p < g_physical_memory.addr_start || (uint) p > g_physical_memory.addr_end) {
        printk("[%s] invalid address: 0x%x\n", __FUNCTION__, p);
        return;
    }

    bitmap_free(&g_physical_memory.bitmap, (uint32) (p - g_physical_memory.addr_start) >> 12);
    if (OPEN_MEMORY_LOG)
        printk("[%s] free page: 0x%X, used: %d pages\n", __FUNCTION__, p, g_physical_memory.bitmap.used);
}
