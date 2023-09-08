//
// Created by toney on 23-9-8.
//

#ifndef TOS_MM_H
#define TOS_MM_H

#include "linux/types.h"

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

void print_checked_memory_info();

#endif //TOS_MM_H
