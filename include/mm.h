//
// Created by toney on 23-9-8.
//

#ifndef TOS_MM_H
#define TOS_MM_H

#include "types.h"

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
    uint16 *map_len;     // 记录位图最大使用长度，理论最大值为4G>>12>>8，即4096，但实际4G物理空间不会全可用
    uint8 *map;          // 用位图记录使用情况，1bit映射一个page
} physical_memory_alloc_t;


void memory_init();
void virtual_memory_init();

uint get_cr3();
void set_cr3(uint v);
void enable_page();

void *alloc_page();
void *alloc_pages(uint count, void **pages);
void free_page(void *p);

#endif //TOS_MM_H
