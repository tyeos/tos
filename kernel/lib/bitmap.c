//
// Created by toney on 23-9-17.
//

#include "../../include/bitmap.h"
#include "../../include/string.h"
#include "../../include/print.h"
#include "../../include/mm.h"

uint32 bitmap_init(bitmap_t *bitmap) {
    uint32 bytes = (bitmap->total + 7) >> 3; // 1个字节可以表示8个物理页的使用情况
    memset(bitmap->map, 0, bytes); // 清零
    bitmap->used = 0;
    bitmap->cursor = 0;
    return bytes;
}

// 申请占用1bit, 成功返回在位图中的位置, 失败范围 ERR_IDX
int bitmap_alloc(bitmap_t *bitmap) {
    if (!bitmap->total) {
        printk("[%s] %p no bits available!\n", __FUNCTION__, bitmap);
        return ERR_IDX;
    }

    if (bitmap->used >= bitmap->total) {
        printk("[%s] %p used up [0x%x / 0x%x]!\n", __FUNCTION__, bitmap, bitmap->used, bitmap->total);
        return ERR_IDX;
    }
    // 查找次数最多为总生效位数
    for (uint32 cursor, index, i = 0; i < bitmap->total; i++) {
        cursor = (bitmap->cursor + i) % bitmap->total; // 全bits索引
        index = cursor / 8; // 全bytes索引
        if (bitmap->map[index] & (1 << cursor % 8)) { // 已分配
            continue;
        }
        bitmap->map[index] |= (1 << cursor % 8); // 尚未分配, 可用
        bitmap->used++; // 进行分配
        bitmap->cursor = cursor + 1;
        if (OPEN_MEMORY_LOG)
            printk("[%s] %p [%d] 0x%x: [0x%X]\n", __FUNCTION__, bitmap, cursor % 8, index, bitmap->map[index]);
        return (int) cursor;
    }
    return ERR_IDX; // 前面已经进行空间预测, 逻辑不会走到这里
}

// 释放占用的1bit, 成功返回在位图中的位置, 失败范围 ERR_IDX
int bitmap_free(bitmap_t *bitmap, uint32 cursor) {
    if (cursor >= bitmap->total) {
        printk("[%s] %p cursor err [0x%X / 0x%X] \n", __FUNCTION__, bitmap, cursor, bitmap->total);
        return ERR_IDX;
    }
    uint index = cursor / 8;
    if (bitmap->map[index] & (1 << cursor % 8)) { // 可回收
        bitmap->map[index] &= ~(1 << cursor % 8);
        bitmap->used--;
        if (OPEN_MEMORY_LOG)
            printk("[%s] %p [%d] 0x%x: [0x%X]\n", __FUNCTION__, bitmap, cursor % 8, index, bitmap->map[index]);
        return (int32) cursor;
    }

    printk("[%s] repeat free %p [%d] 0x%x:[0x%X]\n", __FUNCTION__, bitmap, cursor % 8, index, bitmap->map[index]);
    return ERR_IDX;
}
