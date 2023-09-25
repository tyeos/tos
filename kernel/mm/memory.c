//
// Created by toney on 2023-09-08.
//

#include "../../include/mm.h"
#include "../../include/print.h"
#include "../../include/string.h"
#include "../../include/sys.h"
#include "../../include/bridge/eflags.h"

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
            g_physical_memory.pages_total = g_physical_memory.available_size >> 12; // 页大小4K
            g_physical_memory.pages_used = 0;
            g_physical_memory.alloc_cursor = 0;
            g_physical_memory.bitmap_bytes = (g_physical_memory.pages_total + 7) / 8; // 1个字节可以表示8个物理页的使用情况
            g_physical_memory.bitmap = (uint8 *) PHYSICAL_MEM_BITMAP_ADDR;
            memset(g_physical_memory.bitmap, 0, g_physical_memory.bitmap_bytes); // 清零

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
void *alloc_physical_page() {
    if (g_physical_memory.addr_start != AVAILABLE_ALLOC_MEMORY_FROM) {
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
        if (g_physical_memory.bitmap[index] & (1 << cursor % 8)) {
            continue; // 已分配
        }
        g_physical_memory.bitmap[index] |= (1 << cursor % 8);
        g_physical_memory.pages_used++;
        g_physical_memory.alloc_cursor = cursor + 1;
        void *p = (void *) (g_physical_memory.addr_start + (cursor << 12));
        printk("[%s] alloc page: 0x%X, bits %d: [0x%X], used: %d pages\n", __FUNCTION__, p, index,
               g_physical_memory.bitmap[index], g_physical_memory.pages_used);
        return p;
    }
    // 前面已经进行空间预测, 逻辑不会走到这里
    printk("[%s] memory error!\n", __FUNCTION__);
    return NULL;
}

// 批量分配可用物理页（暂时不用）
static void *alloc_physical_pages(uint count, void **pages) {
    if (g_physical_memory.addr_start != AVAILABLE_ALLOC_MEMORY_FROM || count < 1) {
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
        if (g_physical_memory.bitmap[index] & (1 << cursor % 8)) {
            continue; // 已分配
        }
        // 可用页
        g_physical_memory.bitmap[index] |= (1 << cursor % 8);
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
void free_physical_page(void *p) {
    if ((uint) p < g_physical_memory.addr_start || (uint) p > g_physical_memory.addr_end) {
        printk("[%s] invalid address: 0x%x\n", __FUNCTION__, p);
        return;
    }

    int cursor = (int) (p - g_physical_memory.addr_start) >> 12;
    int index = cursor / 8;
    g_physical_memory.bitmap[index] &= ~(1 << cursor % 8);
    g_physical_memory.pages_used--;

    printk("[%s] free page: 0x%X, bits %d: [0x%X], used: %d pages\n", __FUNCTION__, p, index,
           g_physical_memory.bitmap[index], g_physical_memory.pages_used);
}
