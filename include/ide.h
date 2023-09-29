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
    uint32 start_lba;               // 起始扇区(LBA模式)
    uint32 sec_cnt;                 // 占用的扇区数

    char name[8];                   // 分区名称，如sda1，sda2
    struct chain_elem_t part_tag;   // 用于队列中的元素
    struct super_block *sb;         // 本分区的超级块
    bitmap_t block_bitmap;          // 块位图，文件系统以多个扇区为单位读写，这多个扇区就是块（程序中为了简单，设计一个块一个扇区）
    bitmap_t inode_bitmap;          // i结点位图
    struct chain_t open_inodes;     // 本分区打开的i结点队列
} __attribute__((unused)) partition_t;


/* 硬盘结构 */
typedef struct disk_t {
    struct ide_channel_t *channel;  // 属于哪个IDE通道
    uint8 no;                       // 硬盘号，主盘为0，从盘为1
    char name[8];                   // 本硬盘的名称，如sda，sdb

    partition_t prim_parts[4];      // 主分区，最多4个
    partition_t logic_parts[4];     // 逻辑分区，可以多个，这里设置上限4个
} __attribute__((unused)) disk_t;

/* IDE(/ATA)通道结构 */
typedef struct ide_channel_t {
    uint16 base_port;           // 起始端口
    uint8 int_no;               // 中断向量号
    disk_t disks[2];            // 一个通道上连接两个硬盘，一主一从
    bool waiting;               // 是否正在等待接收硬盘中断
    char name[8];               // 通道名称
    lock_t lock;                // 通道锁
    semaphore_t disk_done;      // 信号量，硬盘工作期间为0，硬盘工作完成后发起中断将其置1，即唤醒驱动程序

} __attribute__((unused)) ide_channel_t;

/* 分区表项结构, 16字节 */
typedef struct partition_table_entry_t {
    uint8 bootable;     // 是否可引导
    uint8 start_head;   // 起始磁头号
    uint8 start_sec;    // 起始扇区号
    uint8 start_chs;    // 起始柱面号
    uint8 fs_type;      // 分区类型
    uint8 end_head;     // 结束磁头号
    uint8 end_sec;      // 结束扇区号
    uint8 end_chs;      // 结束柱面号
    uint32 start_lba;   // 本分区起始扇区的lba地址
    uint32 sec_cnt;     // 本分区的扇区数目
} __attribute__((unused)) partition_table_entry_t;

/* (MBR/EBR)引导扇区结构 */
typedef struct boot_sector_t {
    uint8 boot[446];                    // 引导代码
    partition_table_entry_t tables[4];  // 分区表项，共64字节
    uint16 signature;                   // 启动扇区的结束标志: 0x55, 0xAA
} __attribute__((unused)) boot_sector_t;

disk_t *get_disk(uint8 channel_no, uint8 disk_no);
void ide_read(struct disk_t *disk, uint32 lba, void *buf, uint32 sec_cnt);
void ide_write(struct disk_t *disk, uint32 lba, void *buf, uint32 sec_cnt);

#endif //TOS_IDE_H
