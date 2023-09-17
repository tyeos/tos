//
// Created by toney on 23-9-17.
//

#include "../include/bitmap.h"

// 申请占用1bit, 成功返回在位图中的位置, 失败范围-1
int alloc_bit(bitmap_t *bitmap) {
    if (bitmap->used >= bitmap->total_bits) {
        return -1;
    }
    for (uint cursor, index, i = 0; i < bitmap->total_bits; i++) {
        cursor = (bitmap->cursor + i) % bitmap->total_bits; // 全bits索引
        index = cursor / 8; // 全bytes索引
        if (bitmap->map[index] & (1 << cursor % 8)) { // 已分配
            continue;
        }
        bitmap->map[index] |= (1 << cursor % 8); // 尚未分配, 可用
        return cursor;
    }
    return -1; // 前面已经进行空间预测, 逻辑不会走到这里
}

// 释放占用的1bit, 成功返回在位图中的位置, 失败范围-1
int free_bit(bitmap_t *bitmap, uint cursor) {
    if ((uint) cursor  >= bitmap->total_bits) {
        return -1;
    }
    uint index = cursor / 8;
    if (bitmap->map[index] & (1 << cursor % 8)) { // 可回收
        bitmap->map[index] &= ~(1 << cursor % 8);
        bitmap->used--;
        return cursor;
    }
    return -1;
}
