//
// Created by toney on 2023-09-08.
//

#include "../../include/mm.h"
#include "../../include/print.h"
#include "../../include/string.h"

#define ARDS_TIMES_ADDR 0xE00
#define ARDS_BUFFER_ADDR 0x1000

#define AVAILABLE_MEMORY_FROM 0x100000 // 1M以上作为有效内存

#define MEMORY_BITMAP_LEN_FROM 0xE10   // 内存分配对应位图长度的存储地址
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

        /*
         * 正常情况下, 从0x100000开始的一段内存一定可用.
         * linux下虚拟环境实测有两块内存可用:
         *    第一块从0x0(qemu、bochs)开始, 截至在0x9fbff(qemu) 或 0x9efff(bochs)
         *    第二块从0x100000(qemu、bochs)开始, 截至在0x1efffff(qemu) 或 0x1fdffff(bochs)
         */
        if (!ok && from == AVAILABLE_MEMORY_FROM && p->type == 1) {
            ok = true;

            g_physical_memory.addr_start = from;
            g_physical_memory.addr_end = to;
            g_physical_memory.available_size = len;
            g_physical_memory.pages_total = len >> 12; // 页大小4K
            g_physical_memory.pages_used = 0;
            g_physical_memory.alloc_cursor = 0;
            g_physical_memory.pages_free = g_physical_memory.pages_total - g_physical_memory.pages_used;

            g_physical_memory.map_len = (uint32 *) MEMORY_BITMAP_LEN_FROM;
            g_physical_memory.map = (uint8 *) MEMORY_BITMAP_FROM;
            *g_physical_memory.map_len = (g_physical_memory.pages_total + 7) / 8; // 1个字节可以表示8个物理页的使用情况
            memset(g_physical_memory.map, 0, *g_physical_memory.map_len); // 清零
        }
    }

    if (!ok) {
        printk("[%s:%d] unavailable physical memory!\n", __FILE__, __LINE__);
        return;
    }

    printk("available physical memory pages %d: [0x%x~0x%x]\n", g_physical_memory.pages_total,
           g_physical_memory.addr_start, g_physical_memory.addr_end);
}

// 分配可用物理页
void *alloc_page() {
    if (g_physical_memory.addr_start != AVAILABLE_MEMORY_FROM) {
        printk("[%s] no memory available!\n", __FUNCTION__);
        return NULL;
    }
    if (g_physical_memory.pages_used >= g_physical_memory.pages_total) {
        printk("[%s] memory used up!\n", __FUNCTION__);
        return NULL;
    }
    // 查找次数最多为总页数
    for (uint cursor, index, i = 0; i < g_physical_memory.pages_total; i++) {
        cursor = (g_physical_memory.alloc_cursor + i) % g_physical_memory.pages_total;
        index = cursor / 8;
        if (g_physical_memory.map[index] & (1 << cursor % 8)) {
            continue; // 已分配
        }
        g_physical_memory.map[index] |= (1 << cursor % 8);
        g_physical_memory.pages_used++;
        g_physical_memory.alloc_cursor = cursor + 1;
        void *p = (void *) (g_physical_memory.addr_start + (cursor << 12));
        printk("[%s] alloc page: 0x%X, bits %d: [0x%X], used: %d pages\n", __FUNCTION__, p, index,
               g_physical_memory.map[index], g_physical_memory.pages_used);
        return p;
    }
    // 前面已经进行空间预测, 逻辑不会走到这里
    printk("[%s] memory error!\n", __FUNCTION__);
    return NULL;
}

// 批量分配可用物理页
void *alloc_pages(uint count, void **pages) {
    if (g_physical_memory.addr_start != AVAILABLE_MEMORY_FROM || count < 1) {
        printk("[%s] no memory available!\n", __FUNCTION__);
        return NULL;
    }
    if (g_physical_memory.pages_used + count > g_physical_memory.pages_total) {
        printk("[%s] memory used up!\n", __FUNCTION__);
        return NULL;
    }
    // 查找次数最多为总页数
    for (uint cursor, index, found = 0, i = 0; i < g_physical_memory.pages_total; i++) {
        cursor = (g_physical_memory.alloc_cursor + i) % g_physical_memory.pages_total;
        index = cursor / 8;
        if (g_physical_memory.map[index] & (1 << cursor % 8)) {
            continue; // 已分配
        }
        // 可用页
        g_physical_memory.map[index] |= (1 << cursor % 8);
        g_physical_memory.pages_used++;
        pages[found] = (void *) (g_physical_memory.addr_start + (cursor << 12));
        found++;
        if (found < count) {
            continue;
        }
        // 足量, 更新游标
        g_physical_memory.alloc_cursor = cursor + 1;
        printk("[%s] alloc %d pages: [ ", __FUNCTION__, count);
        if (count < 0x20) {
            for (int j = 0; j < count; ++j) printk("0x%x ", pages[j]);
        } else {
            printk("first:0x%x, last:0x%x ", pages[0], pages[count - 1]);
        }
        printk("], used: %d pages\n", g_physical_memory.pages_used);
        return pages[0]; // 返回首个元素地址
    }
    // 前面已经进行空间预测, 逻辑不会走到这里
    printk("[%s] memory error!\n", __FUNCTION__);
    return NULL;
}

// 释放物理页
void free_page(void *p) {
    if ((uint) p < g_physical_memory.addr_start || (uint) p > g_physical_memory.addr_end) {
        printk("[%s] invalid address!\n", __FUNCTION__);
        return;
    }

    int cursor = (int) (p - g_physical_memory.addr_start) >> 12;
    int index = cursor / 8;
    g_physical_memory.map[index] &= ~(1 << cursor % 8);
    g_physical_memory.pages_used--;

    printk("[%s] free page: 0x%X, bits %d: [0x%X], used: %d pages\n", __FUNCTION__, p, index,
           g_physical_memory.map[index], g_physical_memory.pages_used);
}