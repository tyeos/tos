//
// Created by toney on 23-9-8.
//

#ifndef TOS_MM_H
#define TOS_MM_H

#include "types.h"
#include "bitmap.h"

#define OPEN_MEMORY_LOG false // 是否打开内存调试日志

#define PAGE_SIZE 4096

typedef struct {
    uint32 base_addr_low;    // 内存基地址的低32位
    uint32 base_addr_high;   // 内存基地址的高32位
    uint32 length_low;       // 内存块长度的低32位
    uint32 length_high;      // 内存块长度的高32位
    uint32 type;             // 内存块的类型, 1为可用, 其他为保留
} __attribute__((packed)) ards_t;

typedef struct {
    uint16 times; // 内存检测次数
    ards_t *ards; // 地址范围描述符结构
} __attribute__((packed)) checked_memory_info_t;

/*
 * 地址内存池，所有内存地址分配都用这一套
 */
typedef struct memory_alloc_t {
    uint32 addr_start;      // 有效内存起始地址
    uint32 addr_end;        // 有效内存结束地址
    uint32 available_size;  // 有效内存大小（ addr_end - addr_start + 1 ）
    bitmap_t bitmap;      // 用位图管理内存分配
    uint16 *pde_counter;  // 用来记录页每一个目录项关联的页表数
} __attribute__((packed)) memory_alloc_t;

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
void free_kernel_page(void *v);
void *alloc_kernel_pages(uint32 no);
void free_kernel_pages(void *v, uint32 no);

// 分配、释放用户内存页（虚拟页）
void *alloc_user_page();
void free_user_page(void *p);

// 按字节分配、释放内存（内核虚拟页）
void *kmalloc(size_t size);
void kmfree_s(void *obj, size_t size);
#define kmfree(x) kmfree_s(x, 0)

// 创建虚拟页目录表、用户虚拟地址池初始化（给用户进程用）
void *create_virtual_page_dir();
void user_virtual_memory_alloc_init(memory_alloc_t *user_alloc);

#endif //TOS_MM_H
