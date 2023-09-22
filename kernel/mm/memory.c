//
// Created by toney on 2023-09-08.
//

#include "../../include/mm.h"
#include "../../include/print.h"
#include "../../include/string.h"

#define ARDS_TIMES_ADDR 0xE00
#define ARDS_BUFFER_ADDR 0x1000

/*
 * 1MB以上为有效使用内存, 2MB以上作为有效分配内存
 * 1MB~2MB之间的1MB内存用于存放页目录表及所有的页表
 */
#define AVAILABLE_START_MEMORY_FROM 0x100000
#define AVAILABLE_ALLOC_MEMORY_FROM 0x200000

/*
 * 物理内存按页向外提供
 */
#define PHYSICAL_MEM_BITMAP_LEN_ADDR 0xE10   // 内存分配对应位图长度的存储地址
#define PHYSICAL_MEM_BITMAP_ADDR 0x10000     // 内存分配对应的位图存储起始地址
physical_memory_alloc_t g_physical_memory;

/*
 * 虚拟内存的3GB以下分给用户，高1GB给内核
 * 虚拟内存的低1MB映射到物理内存的低1MB处，1MB~2MB之间的物理内存用作页表，所以不会再一对一映射，即1MB以上的虚拟内存可以随意分配。
 */
#define KERNEL_MEM_BITMAP_LEN_ADDR 0xE14            // 内存分配对应位图长度的存储地址
#define KERNEL_MEM_BITMAP_ADDR 0x30000              // 内核内存池的位图存储位置
#define KERNEL_VIRTUAL_ALLOC_MEMORY_BASE 0xC0100000 // 内核的虚拟地址从哪里开始分配
#define KERNEL_VIRTUAL_ALLOC_MEMORY_MAX_SIZE (0xFFC00000-KERNEL_VIRTUAL_ALLOC_MEMORY_BASE) // 内核的虚拟地址最多可分配多少字节
#define USER_MEM_BITMAP_LEN_ADDR 0xE18              // 内存分配对应位图长度的存储地址
#define USER_MEM_BITMAP_ADDR 0x38000                // 内核内存池的位图存储位置
#define USER_VIRTUAL_ALLOC_MEMORY_BASE 0x100000     // 用户的虚拟地址从哪里开始分配
#define USER_VIRTUAL_ALLOC_MEMORY_MAX_SIZE (0xC0000000-USER_VIRTUAL_ALLOC_MEMORY_BASE) // 用户的虚拟地址最多可分配多少字节
virtual_memory_alloc_t g_virtual_memory;

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
        if (!ok && from == AVAILABLE_START_MEMORY_FROM && to > AVAILABLE_ALLOC_MEMORY_FROM && p->type == 1) {
            ok = true;

            // 物理内存
            g_physical_memory.addr_start = AVAILABLE_ALLOC_MEMORY_FROM;
            g_physical_memory.addr_end = to;
            g_physical_memory.available_size = g_physical_memory.addr_end - g_physical_memory.addr_start + 1;
            g_physical_memory.pages_total = g_physical_memory.available_size >> 12; // 页大小4K
            g_physical_memory.pages_used = 0;
            g_physical_memory.alloc_cursor = 0;
            g_physical_memory.pages_free = g_physical_memory.pages_total;
            g_physical_memory.map_len = (uint32 *) PHYSICAL_MEM_BITMAP_LEN_ADDR;
            *g_physical_memory.map_len = (g_physical_memory.pages_total + 7) / 8; // 1个字节可以表示8个物理页的使用情况
            g_physical_memory.map = (uint8 *) PHYSICAL_MEM_BITMAP_ADDR;
            memset(g_physical_memory.map, 0, *g_physical_memory.map_len); // 清零

            // 虚拟内存（内核空间）
            g_virtual_memory.kernel_addr_start = KERNEL_VIRTUAL_ALLOC_MEMORY_BASE;
            g_virtual_memory.kernel_max_available_size =
                    g_physical_memory.available_size > KERNEL_VIRTUAL_ALLOC_MEMORY_MAX_SIZE
                    ? KERNEL_VIRTUAL_ALLOC_MEMORY_MAX_SIZE : g_physical_memory.available_size;
            g_virtual_memory.kernel_addr_max_end =
                    g_virtual_memory.kernel_addr_start + g_virtual_memory.kernel_max_available_size - 1;
            g_virtual_memory.kernel_alloc_cursor = 0;
            g_virtual_memory.kernel_max_pages = g_virtual_memory.kernel_max_available_size >> 12;
            g_virtual_memory.kernel_map_len = (uint32 *) KERNEL_MEM_BITMAP_LEN_ADDR;
            *g_virtual_memory.kernel_map_len = (g_virtual_memory.kernel_max_pages + 7) / 8; // 1个字节可以表示8个物理页的使用情况
            g_virtual_memory.kernel_map = (uint8 *) KERNEL_MEM_BITMAP_ADDR;
            memset(g_virtual_memory.kernel_map, 0, *g_virtual_memory.kernel_map_len); // 清零

            // 虚拟内存（用户空间）
            g_virtual_memory.user_addr_start = USER_VIRTUAL_ALLOC_MEMORY_BASE;
            g_virtual_memory.user_max_available_size =
                    g_physical_memory.available_size > USER_VIRTUAL_ALLOC_MEMORY_MAX_SIZE
                    ? USER_VIRTUAL_ALLOC_MEMORY_MAX_SIZE : g_physical_memory.available_size;
            g_virtual_memory.user_addr_max_end =
                    g_virtual_memory.user_addr_start + g_virtual_memory.user_max_available_size - 1;
            g_virtual_memory.user_alloc_cursor = 0;
            g_virtual_memory.user_max_pages = g_virtual_memory.user_max_available_size >> 12;
            g_virtual_memory.user_map_len = (uint32 *) USER_MEM_BITMAP_LEN_ADDR;
            *g_virtual_memory.user_map_len = (g_virtual_memory.user_max_pages + 7) / 8; // 1个字节可以表示8个物理页的使用情况
            g_virtual_memory.user_map = (uint8 *) USER_MEM_BITMAP_ADDR;
            memset(g_virtual_memory.user_map, 0, *g_virtual_memory.user_map_len); // 清零

            // 虚拟内存（通用）
            g_virtual_memory.pages_used = 0;
        }
    }

    if (!ok) {
        printk("[%s:%d] unavailable physical memory!\n", __FILE__, __LINE__);
        return;
    }

    printk("available physical memory pages %d: [0x%x~0x%x]\n", g_physical_memory.pages_total,
           g_physical_memory.addr_start, g_physical_memory.addr_end);
    printk("available kernel virtual memory pages %d: [0x%x~0x%x]\n", g_virtual_memory.kernel_max_pages,
           g_virtual_memory.kernel_addr_start, g_virtual_memory.kernel_addr_max_end);
    printk("available user virtual memory pages %d: [0x%x~0x%x]\n", g_virtual_memory.user_max_pages,
           g_virtual_memory.user_addr_start, g_virtual_memory.user_addr_max_end);
}

// 分配可用物理页
static void *palloc_page() {
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
static void *palloc_pages(uint count, void **pages) {
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
static void pfree_page(void *p) {
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

// 分配虚拟页, 不考虑是否挂物理页
static void *valloc_page(enum pool_flags pf) {
    if (pf == PF_KERNEL) {
        if (!g_virtual_memory.kernel_max_pages) {
            printk("[%s] no memory available!\n", __FUNCTION__);
            return NULL;
        }
        if (g_virtual_memory.pages_used >= g_physical_memory.pages_total) { // 分配的虚拟页总数不能大于物理页总数
            printk("[%s] memory used up!\n", __FUNCTION__);
            return NULL;
        }
        // 查找次数最多为总页数
        for (uint cursor, index, i = 0; i < g_virtual_memory.kernel_max_pages; i++) {
            cursor = (g_virtual_memory.kernel_alloc_cursor + i) % g_virtual_memory.kernel_max_pages;
            index = cursor / 8;
            if (g_virtual_memory.kernel_map[index] & (1 << cursor % 8)) {
                continue; // 已分配
            }
            g_virtual_memory.kernel_map[index] |= (1 << cursor % 8);
            g_virtual_memory.pages_used++;
            g_virtual_memory.kernel_alloc_cursor = cursor + 1;
            void *p = (void *) (g_virtual_memory.kernel_addr_start + (cursor << 12));
            printk("[%s] alloc page: 0x%X, bits %d: [0x%X], used: %d pages\n", __FUNCTION__, p, index,
                   g_virtual_memory.kernel_map[index], g_virtual_memory.pages_used);
            return p;
        }
    } else {
        if (!g_virtual_memory.user_max_pages) {
            printk("[%s] no memory available!\n", __FUNCTION__);
            return NULL;
        }
        if (g_virtual_memory.pages_used >= g_physical_memory.pages_total) { // 分配的虚拟页总数不能大于物理页总数
            printk("[%s] memory used up!\n", __FUNCTION__);
            return NULL;
        }
        // 查找次数最多为总页数
        for (uint cursor, index, i = 0; i < g_virtual_memory.user_max_pages; i++) {
            cursor = (g_virtual_memory.user_alloc_cursor + i) % g_virtual_memory.user_max_pages;
            index = cursor / 8;
            if (g_virtual_memory.user_map[index] & (1 << cursor % 8)) {
                continue; // 已分配
            }
            g_virtual_memory.user_map[index] |= (1 << cursor % 8);
            g_virtual_memory.pages_used++;
            g_virtual_memory.user_alloc_cursor = cursor + 1;
            void *p = (void *) (g_virtual_memory.user_addr_start + (cursor << 12));
            printk("[%s] alloc page: 0x%X, bits %d: [0x%X], used: %d pages\n", __FUNCTION__, p, index,
                   g_virtual_memory.user_map[index], g_virtual_memory.pages_used);
            return p;
        }
    }

    // 前面已经进行空间预测, 逻辑不会走到这里
    printk("[%s] memory error!\n", __FUNCTION__);
    return NULL;
}

// 释放虚拟页, 不考虑是否挂物理页
static void vfree_page(void *p, enum pool_flags pf) {
    if (pf == PF_KERNEL) {
        // 内核逻辑
        if ((uint) p < g_virtual_memory.kernel_addr_start || (uint) p > g_virtual_memory.kernel_addr_max_end) {
            printk("[%s] invalid address!\n", __FUNCTION__);
            return;
        }

        int cursor = (int) (p - g_virtual_memory.kernel_addr_start) >> 12;
        int index = cursor / 8;
        g_virtual_memory.kernel_map[index] &= ~(1 << cursor % 8);
        g_virtual_memory.pages_used--;

        printk("[%s] free page: 0x%X, bits %d: [0x%X], used: %d pages\n", __FUNCTION__, p, index,
               g_virtual_memory.kernel_map[index], g_virtual_memory.pages_used);
        return;
    }
    // 用户逻辑
    if ((uint) p < g_virtual_memory.user_addr_start || (uint) p > g_virtual_memory.user_addr_max_end) {
        printk("[%s] invalid address!\n", __FUNCTION__);
        return;
    }

    int cursor = (int) (p - g_virtual_memory.user_addr_start) >> 12;
    int index = cursor / 8;
    g_virtual_memory.user_map[index] &= ~(1 << cursor % 8);
    g_virtual_memory.pages_used--;

    printk("[%s] free page: 0x%X, bits %d: [0x%X], used: %d pages\n", __FUNCTION__, p, index,
           g_virtual_memory.user_map[index], g_virtual_memory.pages_used);
}

// 将虚拟页映射到物理页
static void virtual_binding_physical_page(void *virtual_page, void *physical_page) {
    // 找到一个虚拟地址可以映射到对应的页表项地址
    uint *pte_virtual = (uint *) (0xFFC00000 | (((uint) virtual_page && 0xFFFFF000) >> 10) +
                                               ((((uint) virtual_page >> 12) & 0x3FF) << 2));
    // 修改其值即可
    *pte_virtual = (uint) physical_page | 0b111;
}

// 取消虚拟页到物理页之间的映射关系
static void *virtual_unbinding_physical_page(void *virtual_page) {
    // 找到一个虚拟地址可以映射到对应的页表项地址
    uint *pte_virtual = (uint *) (0xFFC00000 | (((uint) virtual_page && 0xFFFFF000) >> 10) +
                                               ((((uint) virtual_page >> 12) & 0x3FF) << 2));
    void *p = (void *) (*pte_virtual & 0xFFFFF000); // 将其映射的物理页地址返回
    *pte_virtual = 0; // 取消关联
    return p;
}


// 分配虚拟页, 并自动挂载物理页
static void *_alloc_page(enum pool_flags pf) {
    // 申请虚拟地址
    uint *v = (uint *) valloc_page(pf);
    if (!v) {
        return NULL;
    }
    uint *p = (uint *) palloc_page();
    if (!p) {
        vfree_page(v, pf);
        return NULL;
    }
    // 检查虚拟地址是否挂了物理页, 没挂就挂上
    virtual_binding_physical_page(v, p);
    return (void *) v;
}

// 释放虚拟页, 并解除和物理页的关联关系
static void _free_page(void *v, enum pool_flags pf) {
    // 取消虚拟地址和物理地址的映射关系
    void *p = virtual_unbinding_physical_page(v);
    // 释放物理页
    pfree_page(p);
    // 释放虚拟页
    vfree_page(v, pf);
}

// 分配一页内核内存（如果开启了虚拟内存，则返回虚拟地址，并自动挂载物理页）
void *alloc_page() {
    return _alloc_page(PF_KERNEL); // 默认内核申请
}

// 释放分配一页内核内存（如果开启了虚拟内存，则自动解除该虚拟地址和物理页的关联）
void free_page(void *v) {
    _free_page(v, PF_KERNEL); // 默认内核申请
}

// 分配一页用户内存（如果开启了虚拟内存，则返回虚拟地址，并自动挂载物理页）
void *alloc_upage() {
    return _alloc_page(PF_USER); // 默认内核申请
}

// 释放分配一页用户内存（如果开启了虚拟内存，则自动解除该虚拟地址和物理页的关联）
void free_upage(void *v) {
    _free_page(v, PF_USER); // 默认内核申请
}
