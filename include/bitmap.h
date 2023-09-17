//
// Created by toney on 23-9-17.
//

#ifndef TOS_BITMAP_H
#define TOS_BITMAP_H

#include "types.h"

typedef struct bitmap_t {
    uint total_bits;   // 总可分配的位图数量, 即map可用字节数*8
    uint used;         // 已分配的位图数量
    uint cursor;       // 游标, 下一次分配开始检查的bit位置
    uint8 *map;        // 用bit记录使用情况
} bitmap_t;


// 申请占用1bit, 成功返回在位图中的位置, 失败范围-1
int alloc_bit(bitmap_t *bitmap);

// 释放占用的1bit, 成功返回在位图中的位置, 失败范围-1
int free_bit(bitmap_t *bitmap, uint cursor);

#endif //TOS_BITMAP_H
