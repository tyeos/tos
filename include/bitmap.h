//
// Created by toney on 23-9-17.
//

#ifndef TOS_BITMAP_H
#define TOS_BITMAP_H

#include "types.h"

typedef struct bitmap_t {
    uint32 total;       // 总可分配的位图数量, 最大值为map占用字节数*8
    uint32 used;        // 已分配的位图数量
    uint32 cursor;      // 游标, 下一次分配开始检查的bit位置
    uint8 *map;         // 用bit记录使用情况
} bitmap_t;

// bitmap 初始化, 需要先给map和total赋值
uint32 bitmap_init(bitmap_t *bitmap);

// 申请占用1bit, 成功返回在位图中的位置, 失败范围-1
int32 bitmap_alloc(bitmap_t *bitmap);

// 释放占用的1bit, 成功返回在位图中的位置, 失败范围-1
int32 bitmap_free(bitmap_t *bitmap, uint32 cursor);

#endif //TOS_BITMAP_H
