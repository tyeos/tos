//
// Created by toney on 9/28/23.
//


#include "../../include/ide.h"
#include "../../include/print.h"
#include "../../include/io.h"
#include "../../include/string.h"
#include "../../include/eflags.h"



// BIOS会将检测到的硬盘数量写在内存0x475物理地址处
#define HD_NUMBER_MEMORY_PTR 0x475

/*
 * 参考 loader.asm 中 Device寄存器结构
 * 定义硬盘各寄存器的端口号
 */
#define reg_data(channel)     (channel->base_port+0)
#define reg_error(channel)    (channel->base_port+1)
#define reg_sect_cnt(channel) (channel->base_port+2)
#define reg_lba_l(channel)    (channel->base_port+3)
#define reg_lba_m(channel)    (channel->base_port+4)
#define reg_lba_h(channel)    (channel->base_port+5)
#define reg_dev(channel)      (channel->base_port+6)
#define reg_status(channel)   (channel->base_port+7)
#define reg_cmd(channel)      (channel->base_port+7)

#define reg_alt_status(channel) (channel->base_port+0x206)
#define reg_ctl(channel)        (channel->base_port+0x206)

/* 一些硬盘操作的指令 */
#define CMD_IDENTIFY 0xec // identify指令
#define CMD_READ_SECTOR 0x20 // 读扇区指令
#define CMD_WRITE_SECTOR 0x30 // 写扇区指令

/* reg_alt_status寄存器的一些关键位 */
#define BIT_ALT_STAT_BSY 0x80   // 硬盘忙
#define BIT_ALT_STAT_DRDY 0x40  // 驱动器准备好了
#define BIT_ALT_STAT_DRQ 0x8    // 数据传输准备好了


// 两个IDE通道
ide_channel_t channels[2];


/*
 * 硬盘数据结构初始化
 */
void ide_init() {
    // 获取硬盘数量
    uint8 hd_cnt = *((uint8 *) HD_NUMBER_MEMORY_PTR);
    printk("ide: hd_cnt = %d \n", hd_cnt);

    /*
     * 计算的通道数（一个通道两块盘，硬盘按顺序挂载）
     * qume最多挂四块盘，这里的计算结果也就是1或2
     */
    uint8 channel_cnt = (hd_cnt + 1) >> 1;

//    ide_channel_t *channel;
    for (int i = 0; i < channel_cnt; ++i) {
        // 为每个IDE通道初始化端口基址及中断向量号
        ide_channel_t *channel = &channels[i];
        channel->disks[i].d_no = i;
        channel->disks[i].channel = channel;
        switch (i) {
            case 0: // ide0通道
                channel->base_port = 0x1f0;
                channel->int_no = 0x20 + 14; // 从片8259A上倒数第二的中断引脚硬盘
                strcpy(channel->name, "ide0");

                break;
            case 1: // ide1通道
                channel->base_port = 0x170;
                channel->int_no = 0x20 + 15; // 从8259A上最后一个中断引脚
                strcpy(channel->name, "ide1");

                break;
            default:
                printk("ide: error %d\n", i);
                STOP
                break;
        }
        channel->waiting = false;
//        lock_init(&channel->lock);
        /*
         * 初始化为0,目的是向硬盘控制器请求数据后，硬盘驱动 sema_down此信号量会阻塞线程，
         * 直到硬盘完成后通过发中断，由中断处理程序将此信号量sema_up,唤醒线程
         */
//        sema_init(&channel->disk_done, 0);
    }
}


/* 向硬盘控制器写入起始扇区地址及要读写的扇区数 */
static void select_sector(disk_t *disk, uint32 lba, uint8 sec_cnt) {
    ide_channel_t *channel = disk->channel;

    // 写入要读写的扇区数
    outb(reg_sect_cnt(channel), sec_cnt); // sec_cnt为0表示写入256个扇区

    // 写入lba地址，即扇区号
    outb(reg_lba_l(channel), lba);      // lba地址的低8位
    outb(reg_lba_m(channel), lba >> 8); // lba地址的8~15位
    outb(reg_lba_h(channel), lba >> 16);// lba地址的16~23位

    // device寄存器, 参考 loader.asm 中 Device寄存器结构
    // 第5和7位固定为1，第6位MOD=1(LBA), 第4位DEV=0/1(主盘/从盘), 第0~3位为lba地址的24~27位
    outb(reg_dev(channel), (disk->d_no ? 0b11110000 : 0b11100000) | lba >> 24);
}


/* 向通道channel发cmd命令 */
static void cmd_out(ide_channel_t *channel, uint8 cmd) {
    channel->waiting = true;
    outb(reg_cmd(channel), cmd);
}

/* 硬盘读入sec_cnt个扇区的数据到buf */
static void read_sector(disk_t *disk, void *buf, uint8 sec_cnt) {
    insw(reg_data(disk->channel), buf, (sec_cnt ? sec_cnt : 256) << 9 >> 1);  // 1扇区512字节, 按字读入
}

/* 将buf中sec_cnt扇区的数据写入硬盘 */
static void write_sector(disk_t *disk, void *buf, uint8 sec_cnt) {
    outsw(reg_data(disk->channel), buf, (sec_cnt ? sec_cnt : 256) << 9 >> 1);  // 1扇区512字节, 按字写出
}

/*
 * 等待硬盘响应
 * 在ata手册中有这么一句话：“All actions required in this state shall be completed within 31s",
 * 大概意思是所有的操作都应该在31秒内完成，所以我们在30秒内等待硬盘响应，若成功则返回true
 */
static bool busy_wait(disk_t *disk) {
    ide_channel_t *channel = disk->channel;
    uint16 time_limit = 30 * 1000;  // 可以等待30000毫秒
    while (time_limit -= 10 >= 0) {
        if (!(inb(reg_status(channel)) & BIT_ALT_STAT_BSY)) {
            return (inb(reg_status(channel)) & BIT_ALT_STAT_DRQ);
        }
        SLEEP_MS(10) // 睡眠10毫秒
    }
    return false;
}


#define TEST_READ_SEC_CNT 4

void test_read_disk() {
    ide_init();

    select_sector(&channels[0].disks[0], 0, TEST_READ_SEC_CNT);
    cmd_out(&channels[0], CMD_READ_SECTOR);
}

char buff[TEST_READ_SEC_CNT << 9] = {0};

void hd_handler() {
    read_sector(&channels[0].disks[0], buff, TEST_READ_SEC_CNT);
    uint32 size = TEST_READ_SEC_CNT << 9;
    for (int i = 0; i < size; ++i) {
        if (i % 16 == 0) printk("\n");
        printk("%02x ", buff[i] & 0xff);
    }
    printk("\nhd_handler end~ \n");
}

