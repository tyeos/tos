//
// Created by toney on 23-9-17.
//

#ifndef TOS_BITMAP_H
#define TOS_BITMAP_H

#include "types.h"

typedef struct bitmap_t {
    uint32 total;       // 总可分配的位图数量, 最大值为map占用字节数*8
    uint32 used;        // 已分配的位图数量
    uint32 fast;        // 快速指针, 记录未分配bit的起始位置，正常分配从0开始扫描，快速分配直接走空闲位置划一块出去，直到最后
    uint8 *map;         // 用bit记录使用情况
} __attribute__((packed)) bitmap_t;

// bitmap 初始化, 需要先给map和total赋值, 会清空map
uint32 bitmap_init(bitmap_t *bitmap);

// bitmap 慢速初始化, 需要先给map和total赋值, 会读取map, 在其基础上进行初始化（由于要读原位图，所以速度相对慢一些）
void bitmap_slow_init(bitmap_t *bitmap);

// 申请占用1bit, 成功返回在位图中的位置, 失败范围-1
uint32 bitmap_alloc(bitmap_t *bitmap);

// 释放占用的1bit, 成功返回在位图中的位置, 失败范围-1
uint32 bitmap_free(bitmap_t *bitmap, uint32 bit_idx);

// 按块申请连续位, 成功返回在位图中的起始位置, 失败返回 ERR_IDX
uint32 bitmap_alloc_block(bitmap_t *bitmap, uint32 bits);

// 按块释放连续位, 成功返回在位图中的起始位置, 失败返回 ERR_IDX
uint32 bitmap_free_block(bitmap_t *bitmap, uint32 bit_idx, uint32 bits);

#endif //TOS_BITMAP_H
