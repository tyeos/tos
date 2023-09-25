//
// Created by toney on 23-9-8.
//

#ifndef TOS_MM_H
#define TOS_MM_H

#include "types.h"

#define PAGE_SIZE 4096

typedef struct {
    uint32 base_addr_low;    // 内存基地址的低32位
    uint32 base_addr_high;   // 内存基地址的高32位
    uint32 length_low;       // 内存块长度的低32位
    uint32 length_high;      // 内存块长度的高32位
    uint32 type;             // 内存块的类型, 1为可用, 其他为保留
} ards_t;

typedef struct {
    uint16 times; // 内存检测次数
    ards_t *ards; // 地址范围描述符结构
} checked_memory_info_t;

/*
 * 地址内存池，所有内存地址分配都用这一套
 */
typedef struct memory_alloc_t {
    uint addr_start;     // 有效内存起始地址
    uint addr_end;       // 有效内存结束地址
    uint available_size; // 有效内存大小（ addr_end - addr_start + 1 ）
    uint pages_total;    // 有效内存页数 ( available_size >> 12 )
    uint pages_used;     // 已分配的内存页数
    uint alloc_cursor;   // 内存分配游标, 指向下一次分配page在位图中对应bit的位置
    uint bitmap_bytes;   // 位图占用字节数, 物理内存理论最大值为4GB>>12>>3, 即128KB=131072, 实际4G物理空间不可能全允许分配使用
    uint8 *bitmap;       // 位图，记录页分配情况，1bit映射一个page, 1byte映射8个page
} memory_alloc_t;

/*
 * 内存池标记，用于判断用哪个内存池
 * 这里只针对虚拟地址
 */
enum pool_flags {
    PF_KERNEL = 1,  // 内核内存池
    PF_USER = 2     // 用户内存池
};

uint get_cr3();
void set_cr3(uint v);
void enable_page();

// 分配、释放物理页
void *alloc_physical_page();
void free_physical_page(void *p);

// 分配、释放内核内存页（虚拟页）
void *alloc_kernel_page();
void free_kernel_page(void *p);

// 分配、释放用户内存页（虚拟页）
void *alloc_user_page();
void free_user_page(void *p);

// 将虚拟地址转为物理地址
void *v2p_addr(void *vaddr);

// 按字节分配、释放内存（内核虚拟页）
void *kmalloc(size_t size);
void kmfree_s(void *obj, int size);
#define kmfree(x) kmfree_s(x, 0)

#endif //TOS_MM_H
