//
// Created by toney on 2023-09-02.
//

#ifndef TOS_DT_H
#define TOS_DT_H

#include "types.h"

#define PAGE_DIR_PHYSICAL_ADDR 0x100000         // 页目录表物理地址, 低1MB空间之上的第一个字节
#define PAGE_TAB_ITEM0_PHYSICAL_ADDR 0x101000   // 第0个页表物理地址, 页表空间之上的第一个字节

// 全局描述符（全局描述符表项）结构
typedef struct global_descriptor {
    uint16 limit_low;     // 段界限的 0 ~ 15 位
    uint32 base_low: 24;  // 段基址的 0 ~ 23 位
    uint8 type: 4;        // 内存段类型
    uint8 segment: 1;     // S位，1 表示代码段或数据段，0 表示系统段
    uint8 DPL: 2;         // Descriptor Privilege Level 描述符特权等级 0 ~ 3
    uint8 present: 1;     // P位，存在位，1 在内存中，0 在磁盘上/段异常
    uint8 limit_high: 4;  // 段界限的 16 ~ 19 位
    uint8 available: 1;   // AVL位，没有专门用途
    uint8 long_mode: 1;   // L位，64 位扩展标志，0为32位代码段，1为64位代码段
    uint8 big: 1;         // D/B位，32 位（值为1） 或 16 位（值为0）
    uint8 granularity: 1; // G位，粒度 4KB（值为1） 或 1B（值为0）
    uint8 base_high;      // 段基址的 24 ~ 31 位
} __attribute__((packed)) global_descriptor;

// 描述符选择子结构
typedef struct descriptor_selector {
    uint8 RPL: 2;     // Request Privilege Level, 请求特权级，0~3
    uint8 TI: 1;      // Table Indicator, 0-在GDT中索引描述符，1-在LDT中索引描述符
    uint16 index: 13; // 描述符索引值
} __attribute__((packed)) descriptor_selector;

// 中断门（中断描述符表项）结构
typedef struct interrupt_gate {
    uint16 offset0;    // 目标代码段内偏移的 0 ~ 15 位
    uint16 selector;   // 目标代码段选择子
    uint8 reserved;    // 保留不用
    uint8 type: 4;     // 系统段类型，如：任务门/中断门/陷阱门/调用门 等
    uint8 segment: 1;  // S位，0表示系统段
    uint8 DPL: 2;      // 使用 int 指令访问的最低权限
    uint8 present: 1;  // P位，标识该门是否有效
    uint16 offset1;    // 目标代码段内偏移的 16 ~ 31 位
} __attribute__((packed)) interrupt_gate;

// 描述符表寄存器内存数据结构
#pragma pack(2)
typedef struct dt_ptr {
    uint16 limit; // 描述符表界限
    uint32 base;    // 描述符表基址
} __attribute__((packed)) dt_ptr;
#pragma pack()

void update_tss_esp(uint32 esp0);

#endif //TOS_DT_H
