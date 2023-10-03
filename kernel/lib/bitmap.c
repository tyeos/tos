//
// Created by toney on 23-9-17.
//

#include "../../include/bitmap.h"
#include "../../include/string.h"
#include "../../include/print.h"
#include "../../include/mm.h"
#include "../../include/sys.h"

// bitmap 初始化, 需要先给map和total赋值
uint32 bitmap_init(bitmap_t *bitmap) {
    uint32 bytes = (bitmap->total + 7) >> 3; // 1个字节可以表示8个物理页的使用情况
    memset(bitmap->map, 0, bytes); // 清零
    bitmap->used = 0;
    bitmap->fast = 0;
    return bytes;
}

// bitmap 慢速初始化，即直接使用原位图，在其基础上进行初始化
void bitmap_slow_init(bitmap_t *bitmap) {
    bitmap->used = 0;   // 已使用数无法确定
    bitmap->fast = 0;   // 停用按块申请

    // 走fast指针回收的逻辑
    uint32 shadow_fast = bitmap->total + 1; // 默认指向最后
    while (shadow_fast) {
        // 检查是否可按整字节回收
        if (shadow_fast % 8 == 0 && bitmap->map[(shadow_fast >> 3) - 1] == 0) {
            shadow_fast -= 8;
            continue;
        }
        // 检查上一位是否可回收
        if (bitmap->map[(shadow_fast - 1) >> 3] & (1 << (shadow_fast - 1) % 8)) {
            bitmap->used++;     // 该位为1，不可回收
            if (!bitmap->fast) bitmap->fast = shadow_fast; // 如果fast为0则赋值
        }
        shadow_fast--;
    }
}

// 申请占用1bit, 成功返回在位图中的位置, 失败返回 ERR_IDX
uint32 bitmap_alloc(bitmap_t *bitmap) {
    // 必须先初始化
    if (!bitmap->total) {
        printk("[%s] %p no bits available!\n", __FUNCTION__, bitmap);
        STOP
        return ERR_IDX;
    }
    // 保证有可用位
    if (bitmap->used >= bitmap->total) {
        printk("[%s] %p used up [%d / %d]!\n", __FUNCTION__, bitmap, bitmap->used, bitmap->total);
        STOP
        return ERR_IDX;
    }

    // 每次从0开始扫描, 最多扫描次数为总bit数, 暂不考虑效率问题（优化可以加一个慢速指针，在已用区域循环扫描，限制次数内还未找到可跳至快速指针）
    for (uint32 bit_idx = 0, byte_idx; bit_idx < bitmap->total; bit_idx++) {
        byte_idx = bit_idx >> 3; // 全bytes索引
        if (bitmap->map[byte_idx] & (1 << bit_idx % 8)) continue; // 已分配
        bitmap->map[byte_idx] |= (1 << bit_idx % 8); // 尚未分配, 可用, 进行分配
        bitmap->used++; // 使用数+1
        if (bit_idx >= bitmap->fast) bitmap->fast = bit_idx + 1; // 保证fast始终指向空闲区的起始位置

        if (OPEN_MEMORY_LOG)
            printk("[%s] %p [%d / %d]\n", __FUNCTION__, bitmap, bit_idx, bitmap->total - 1);

        return bit_idx;
    }

    // 前面已经进行空间预测, 逻辑不会走到这里
    printk("[%s] %p err [%d / %d]!\n", __FUNCTION__, bitmap, bitmap->used, bitmap->total);
    STOP
    return ERR_IDX;
}

// 按块申请连续位, 成功返回在位图中的起始位置, 失败返回 ERR_IDX
uint32 bitmap_alloc_block(bitmap_t *bitmap, uint32 bits) {
    if (bits == 1) return bitmap_alloc(bitmap);

    // 必须先初始化
    if (!bitmap->total) {
        printk("[%s] %p no bits available!\n", __FUNCTION__, bitmap);
        STOP
        return ERR_IDX;
    }
    // 保证有连续可用位, 这里简单处理, 只从空闲区划一块出去，不从头扫描, 所以不考虑间断分配的内存（可能间断的内存中确实有可用的连续空间）
    if (bitmap->fast + bits > bitmap->total) {
        printk("[%s] %p used block up [%d~%d / %d]!\n", __FUNCTION__, bitmap, bitmap->fast,
               bitmap->fast + bits - 1, bitmap->total - 1);
        STOP
        return ERR_IDX;
    }

    // 从规划上保证从fast开始一直到total都是空闲区, 所以这里将fast开始的后续字节全部置为1即可

    uint32 bit_idx = bitmap->fast;              // 分配的起始bit位置
    uint32 byte_idx = bit_idx >> 3;             // 分配的起始byte位置
    uint8 byte_bit_idx = bitmap->fast % 8;      // 分配的起始bit在byte中的位置
    uint8 byte_rest_bits = 8 - byte_bit_idx;    // 当前byte中待分配的连续bit数

    if (bits <= byte_rest_bits) {
        // 当前这个字节中的剩余bits就够用, 直接分配
        bitmap->map[byte_idx] |= ((1 << (byte_bit_idx + bits)) - 1) - ((1 << byte_bit_idx) - 1);
    } else {
        // 当前字节不够用，那除了当前字节的剩余位需要分配外，还需要继续往后分配后面字节的空间

        // 先将当前字节剩余空间分配出去
        bitmap->map[byte_idx] |= 0xFF - ((1 << byte_bit_idx) - 1);

        // 再将需要分配的整字节分配出去
        uint32 continue_bytes = (bits - byte_rest_bits) >> 3;
        for (uint32 i = 0; i < continue_bytes; ++i) {
            bitmap->map[byte_idx + 1 + i] = 0xFF;
        }
        // 最后剩余的bit数
        byte_rest_bits = (bits - byte_rest_bits) % 8;
        if (byte_rest_bits) {
            bitmap->map[byte_idx + 1 + continue_bytes] |= (1 << byte_rest_bits) - 1; // 如果剩余待分配的bit数大于0，则继续分配
        }
    }

    // 保存结果，返回
    bitmap->fast += bits;
    bitmap->used += bits;

    if (OPEN_MEMORY_LOG)
        printk("[%s] %p [%d ~ %d / %d]\n", __FUNCTION__, bitmap, bit_idx, bit_idx + bits - 1, bitmap->total - 1);

    return bit_idx;
}

// 检查缩减fast指针
static void check_reduce_fast(bitmap_t *bitmap) {
    while (bitmap->fast) {
        // 检查是否可按整字节回收
        if (bitmap->fast % 8 == 0 && bitmap->map[(bitmap->fast >> 3) - 1] == 0) {
            bitmap->fast -= 8;
            continue;
        }
        // 检查上一位是否可回收
        if (bitmap->map[(bitmap->fast - 1) >> 3] & (1 << (bitmap->fast - 1) % 8)) return; // 该位为1，不可回收
        bitmap->fast--;
    }
}

// 释放占用的1bit, 成功返回在位图中的位置, 失败返回 ERR_IDX
uint32 bitmap_free(bitmap_t *bitmap, uint32 bit_idx) {
    if (bit_idx >= bitmap->total) {
        printk("[%s] %p out [%d / %d] \n", __FUNCTION__, bitmap, bit_idx, bitmap->total - 1);
        STOP
        return ERR_IDX;
    }
    uint byte_idx = bit_idx >> 3;
    if (bitmap->map[byte_idx] & (1 << bit_idx % 8)) { // 可回收
        bitmap->map[byte_idx] &= ~(1 << bit_idx % 8);
        bitmap->used--;
        check_reduce_fast(bitmap);

        if (OPEN_MEMORY_LOG)
            printk("[%s] %p [%d / %d]\n", __FUNCTION__, bitmap, bit_idx, bitmap->total - 1);
        return bit_idx;
    }

    printk("[%s] repeat %p [%d / %d]\n", __FUNCTION__, bitmap, bit_idx, bitmap->total - 1);
    STOP
    return ERR_IDX;
}

// 按块释放连续位, 成功返回在位图中的起始位置, 失败返回 ERR_IDX
uint32 bitmap_free_block(bitmap_t *bitmap, uint32 bit_idx, uint32 bits) {
    if (bits == 1) return bitmap_free(bitmap, bit_idx);

    if (bit_idx + bits > bitmap->total) {
        printk("[%s] %p out [%d~%d / %d] \n", __FUNCTION__, bitmap, bit_idx, bit_idx + bits - 1,
               bitmap->total - 1);
        STOP
        return ERR_IDX;
    }
    uint byte_idx = bit_idx >> 3;               // 起始byte位置
    uint8 byte_bit_idx = bit_idx % 8;           // 起始bit在byte中的位置
    uint8 byte_rest_bits = 8 - byte_bit_idx;    // 当前byte中最多可连续释放的bit数

    if (bits <= byte_rest_bits) {
        // 当前这个字节中的剩余bits就够释放
        uint8 check = ~(1 << (bit_idx + bits) | (1 << bit_idx) - 1); // 校验值
        if (check != (bitmap->map[byte_idx] & check)) { // 保证这几位全为1
            printk("[%s] repeat %p [%d~%d / %d]\n", __FUNCTION__, bitmap, bit_idx, bit_idx + bits,
                   bitmap->total - 1);
            STOP
            return ERR_IDX;
        }
        bitmap->map[byte_idx] &= ~check; // 置零即释放
    } else {
        // 当前字节不够用，那除了当前字节的剩余位需要释放外，还需要继续往后释放后面字节的空间

        // 先将当前字节剩余空间释放
        uint8 check = (int8) 0x80 >> (byte_rest_bits - 1); // 校验值, 有符号右移高位补1
        if (check != (bitmap->map[byte_idx] & check)) { // 保证这几位全为1
            printk("[%s] repeat %p [%d~%d / %d~%d / %d]\n", __FUNCTION__, bitmap, bit_idx, bit_idx + byte_rest_bits,
                   bit_idx, bit_idx + bits, bitmap->total - 1);
            STOP
            return ERR_IDX;
        }
        bitmap->map[byte_idx] &= ~check; // 置零即释放

        // 再将整字节释放
        uint32 continue_bytes = (bits - byte_rest_bits) >> 3;
        for (uint32 i = 0; i < continue_bytes; ++i) {
            if (bitmap->map[byte_idx + 1 + i] != 0xFF) { // 保证这几位全为1
                printk("[%s] repeat %p [%d~%d / %d~%d / %d]\n", __FUNCTION__, bitmap, (byte_idx + 1 + i) << 3,
                       ((byte_idx + 1 + i) << 3) + 7, bit_idx, bit_idx + bits, bitmap->total - 1);
                STOP
                return ERR_IDX;
            }
            bitmap->map[byte_idx + 1 + i] = 0;
        }

        // 最后剩余的bit数
        byte_rest_bits = (bits - byte_rest_bits) % 8;
        if (byte_rest_bits) {
            check = (1 << byte_rest_bits) - 1; // 校验值
            if (check != (bitmap->map[byte_idx + 1 + continue_bytes] & check)) { // 保证这几位全为1
                printk("[%s] repeat %p [%d~%d / %d~%d / %d]\n", __FUNCTION__, bitmap,
                       (byte_idx + 1 + continue_bytes) << 3, ((byte_idx + 1 + continue_bytes) << 3) + byte_rest_bits,
                       bit_idx, bit_idx + bits, bitmap->total - 1);
                STOP
                return ERR_IDX;
            }
            bitmap->map[byte_idx + 1 + continue_bytes] &= ~check; // 置零即释放
        }
    }
    bitmap->used -= bits;
    check_reduce_fast(bitmap);

    if (OPEN_MEMORY_LOG)
        printk("[%s] %p [%d~%d / %d]\n", __FUNCTION__, bitmap, bit_idx, bit_idx + bits - 1, bitmap->total - 1);

    return bit_idx;
}


