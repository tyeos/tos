//
// Created by toney on 2023-09-02.
//

#ifndef TOS_DT_H
#define TOS_DT_H

// 全局描述符（全局描述符表项）结构
typedef struct global_descriptor {
    unsigned short limit_low;     // 段界限的 0 ~ 15 位
    unsigned int base_low: 24;    // 段基址的 0 ~ 23 位
    unsigned char type: 4;        // 内存段类型
    unsigned char segment: 1;     // S位，1 表示代码段或数据段，0 表示系统段
    unsigned char DPL: 2;         // Descriptor Privilege Level 描述符特权等级 0 ~ 3
    unsigned char present: 1;     // P位，存在位，1 在内存中，0 在磁盘上/段异常
    unsigned char limit_high: 4;  // 段界限的 16 ~ 19 位
    unsigned char available: 1;   // AVL位，没有专门用途
    unsigned char long_mode: 1;   // L位，64 位扩展标志，0为32位代码段，1为64位代码段
    unsigned char big: 1;         // D/B位，32 位（值为1） 或 16 位（值为0）
    unsigned char granularity: 1; // G位，粒度 4KB（值为1） 或 1B（值为0）
    unsigned char base_high;      // 段基址的 24 ~ 31 位
} __attribute__((packed)) global_descriptor;

// 描述符选择子结构
typedef struct descriptor_selector {
    char RPL: 2;     // Request Privilege Level, 请求特权级，0~3
    char TI: 1;      // Table Indicator, 0-在GDT中索引描述符，1-在LDT中索引描述符
    short index: 13; // 描述符索引值
} __attribute__((packed)) descriptor_selector;

// 中断门（中断描述符表项）结构
typedef struct interrupt_gate {
    short offset0;    // 目标代码段内偏移的 0 ~ 15 位
    short selector;   // 目标代码段选择子
    char reserved;    // 保留不用
    char type: 4;     // 系统段类型，如：任务门/中断门/陷阱门/调用门 等
    char segment: 1;  // S位，0表示系统段
    char DPL: 2;      // 使用 int 指令访问的最低权限
    char present: 1;  // P位，标识该门是否有效
    short offset1;    // 目标代码段内偏移的 16 ~ 31 位
} __attribute__((packed)) interrupt_gate;

// 描述符表寄存器内存数据结构
#pragma pack(2)
typedef struct dt_ptr {
    short limit; // 描述符表界限
    int base;    // 描述符表基址
} dt_ptr;
#pragma pack()

#endif //TOS_DT_H
