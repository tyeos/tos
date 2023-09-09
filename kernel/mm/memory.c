//
// Created by toney on 2023-09-08.
//

#include "../../include/mm.h"
#include "../../include/print.h"
#include "../../include/string.h"

#define ARDS_TIMES_ADDR 0x1100
#define ARDS_BUFFER_ADDR 0x1102

#define AVAILABLE_MEMORY_FROM 0x100000 // 1M以上作为有效内存
#define MEMORY_BITMAP_FROM 0x10000     // 内存分配对应的位图存储起始地址

physical_memory_alloc_t g_physical_memory;

void memory_init() {
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

        // 一般情况下有两块内存可用，第一块为0x0~0x9FBFF, 第二块为0x100000~0x1fdffff
        if (!ok && from == AVAILABLE_MEMORY_FROM && p->type == 1) {
            ok = true;

            g_physical_memory.addr_start = from;
            g_physical_memory.addr_end = to;
            g_physical_memory.available_size = len;
            g_physical_memory.pages_total = len >> 12; // 页大小4K
            g_physical_memory.pages_used = 0;
            g_physical_memory.alloc_cursor = 0;
            g_physical_memory.pages_free = g_physical_memory.pages_total - g_physical_memory.pages_used;

            g_physical_memory.map = (uint8 *) MEMORY_BITMAP_FROM;
            memset(g_physical_memory.map, 0, g_physical_memory.pages_total); // 清零
        }
    }

    if (!ok) {
        printk("[%s:%d] unavailable physical memory!\n", __FILE__, __LINE__);
        return;
    }

    printk("available physical memory pages %d: [0x%x~0x%x]\n", g_physical_memory.pages_total,
           g_physical_memory.addr_start, g_physical_memory.addr_end);
}


void *alloc_page() {
    if (g_physical_memory.addr_start != AVAILABLE_MEMORY_FROM) {
        printk("no memory available!\n");
        return NULL;
    }

    if (g_physical_memory.pages_used >= g_physical_memory.pages_total) {
        printk("memory used up!\n");
        return NULL;
    }

    bool found = false;
    uint index = g_physical_memory.alloc_cursor;
    for (uint i = 0; i < g_physical_memory.pages_total; i++) {
        index = (index + i) % g_physical_memory.pages_total;
        if (g_physical_memory.map[index] == 0) {
            found = true;
            break;
        }
    }
    if (!found) {
        printk("memory error!\n");
        return NULL;
    }
    g_physical_memory.map[index] = 1;
    g_physical_memory.alloc_cursor = index;
    g_physical_memory.pages_used++;

    void *p = (void *) (g_physical_memory.addr_start + (index << 12));
    printk("[%s] alloc page: 0x%X, used: %d pages\n", __FUNCTION__, p, g_physical_memory.pages_used);
    return p;
}

void free_page(void *p) {
    if ((uint) p < g_physical_memory.addr_start || (uint) p > g_physical_memory.addr_end) {
        printk("invalid address!\n");
        return;
    }

    int index = (int) (p - g_physical_memory.addr_start) >> 12;
    g_physical_memory.map[index] = 0;
    g_physical_memory.pages_used--;

    printk("[%s] free page: 0x%X, used: %d pages\n", __FUNCTION__, p, g_physical_memory.pages_used);
}