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


typedef struct {
    uint addr_start;     // 可用内存起始地址
    uint addr_end;       // 可用内存结束地址
    uint available_size; // 可用内存大小
    uint pages_total;    // 机器物理内存共多少page
    uint pages_free;     // 机器物理内存还剩多少page
    uint pages_used;     // 机器物理内存用了多少page
    uint alloc_cursor;   // 内存分配游标
    uint32 *map_len;     // 记录位图最大使用长度，理论最大值为4GB>>12>>3，即128KB=131072，但实际4G物理空间不可能全允许分配使用, 虚拟机上更是远小于该理论值
    uint8 *map;          // 用位图记录使用情况，1bit映射一个page, 1byte映射8个page
} physical_memory_alloc_t;


typedef struct {
    uint kernel_addr_start;     // 内核虚拟内存分配的起始地址
    uint kernel_max_available_size; // 内核最大可分配的虚拟内存大小
    uint kernel_addr_max_end;       // 内核虚拟内存分配的最大结束地址
    uint kernel_max_pages; // 内核虚拟内存最多可以有多少page
    uint kernel_alloc_cursor;   // 内核虚拟内存分配游标

    uint user_addr_start;       // 用户虚拟内存分配的起始地址
    uint user_max_available_size;   // 用户最大可分配的虚拟内存大小
    uint user_addr_max_end;         // 用户虚拟内存分配的最大结束地址
    uint user_max_pages; // 用户虚拟内存最多可以有多少page
    uint user_alloc_cursor;     // 用户虚拟内存分配游标

    uint pages_used;            // 虚拟内存已经分配了多少page（用户+内核）

    uint32 *kernel_map_len;     // 内核位图占用的字节长度
    uint32 *user_map_len;       // 用户位图占用的字节长度

    uint8 *kernel_map;          // 内核位图，记录内存使用情况，1bit映射一个page, 1byte映射8个page
    uint8 *user_map;            // 用户位图，记录内存使用情况，1bit映射一个page, 1byte映射8个page

} virtual_memory_alloc_t;

// 内存池标记，用于判断用哪个内存池
enum pool_flags {
    PF_KERNEL = 1,  //内核内存池
    PF_USER = 2     //用户内存池
};

void memory_init();
void virtual_memory_init();

uint get_cr3();
void set_cr3(uint v);
void enable_page();

// 分配、释放内核内存页
void *alloc_page();
void free_page(void *p);

// 分配、释放用户内存页
void *alloc_upage();
void free_upage(void *v);

// 按字节分配、释放内存
void *malloc(size_t size);
void free_s(void *obj, int size);
#define mfree(x,i) free_s((x), i)
#define free(x) free_s(x, 0)


#endif //TOS_MM_H
