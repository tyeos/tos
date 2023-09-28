//
// Created by toney on 9/28/23.
//

#ifndef TOS_IDE_H
#define TOS_IDE_H

#include "types.h"
#include "chain.h"
#include "bitmap.h"
#include "lock.h"

/*
 * IDE (Integrated Device Electronics) 也称 ATA（AT Attachment）。
 *
 * IDE(/ATA)通道一共有两个，每个通道可挂载两块硬盘（主盘和从盘）：
 *      Primary通道：
 *          从8259A从片IRQ14接收硬盘中断（8259A从片级联在主片的IRQ2接口）
 *          命令块寄存器端口范围是0x1F0~0x1F7
 *          控制块寄存器端口是0x3F6, 即 0x1F0 + 0x206
 *      Secondary通道：
 *          从8259A从片IRQ15接收硬盘中断
 *          命令块寄存器端口范围是0x170~0x177
 *          控制块寄存器端口是0x376, 即 0x170 + 0x206
 *
 * device寄存器:
 *      dev位(第4位)可指定对主盘还是从盘操作
 */

/* 分区结构 */
typedef struct partition_t {
    struct disk_t *disk;            // 属于哪个硬盘
    uint32 sector_start;            // 起始扇区(LBA模式)
    uint32 sector_count;            // 占用的扇区数

    char name[8];                   // 分区名称，如sda1，sda2
    struct chain_elem_t chain_elem; // 用于队列中的元素
    struct super_block *sb;         // 本分区的超级块
    bitmap_t block_bitmap;          // 块位图，文件系统以多个扇区为单位读写，这多个扇区就是块（程序中为了简单，设计一个块一个扇区）
    bitmap_t inode_bitmap;          // i结点位图
    struct chain_t open_inodes;     // 本分区打开的i结点队列
} __attribute__((unused)) partition_t;


/* 硬盘结构 */
typedef struct disk_t {
    struct ide_channel_t *channel;  // 属于哪个IDE通道
    uint8 d_no;                     // 硬盘号，主盘为0，从盘为1
    partition_t prim_parts[4];      // 主分区，最多4个
    partition_t logic_parts[4];     // 逻辑分区，可以多个，这里设置上限4个

    char name[8];                   // 本硬盘的名称，如sda，sdb
} __attribute__((unused)) disk_t;

/* IDE(/ATA)通道结构 */
typedef struct ide_channel_t {
    uint16 base_port;           // 起始端口
    uint8 int_no;               // 中断向量号
    disk_t disks[2];            // 一个通道上连接两个硬盘，一主一从
    bool waiting;               // 是否正在等待接收硬盘中断

    semaphore_t disk_done;      // 信号量，等待硬盘工作期间通过此信号阻塞自己，硬盘工作完成后发起中断，再通过此信号唤醒驱动程序
    lock_t lock;                // 通道锁
    char name[8];               // 通道名称

} __attribute__((unused)) ide_channel_t;


#endif //TOS_IDE_H
