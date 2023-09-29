//
// Created by toney on 9/28/23.
//


#include "../../include/ide.h"
#include "../../include/print.h"
#include "../../include/io.h"
#include "../../include/string.h"
#include "../../include/eflags.h"


/*
 * 1，读取硬盘信息，包括：
 *      1.1, 获取硬盘数量
 *          1.1.1，IDE通道信息
 *          1.1.2，每个通道上的硬盘信息
 *      1.2，每个硬盘上的引导扇区信息
 *          1.2.1，每个引导扇区的分区表信息
 *          1.2.2，每个分区表的分区表项信息
 *
 * 2，对硬盘操作，包括：
 *      2.1，读盘（以扇区为单位）
 *      2.2，写盘（以扇区为单位）
 *
 */

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


// 最多两个IDE通道
static ide_channel_t channels[2];
static uint8 channel_cnt = 0;
// 硬盘总数量
static uint8 hd_cnt = 0;


// 获取硬盘数量
static void hd_count_init() {
    hd_cnt = *((uint8 *) HD_NUMBER_MEMORY_PTR);
    // 这里简单计算，即硬盘按顺序挂载，前两块盘挂主通道，从第三块盘开始挂从通道
    channel_cnt = hd_cnt > 2 ? 2 : 1;
    printk("ide: hd_cnt = %d \n", hd_cnt);
}

/*
 * Linux的硬盘命名规则是[x]d[y][n], 其中只有字母d是固定的，其他带中括号的字符都是多选值：
 *      x表示硬盘分类，硬盘有两大类，IDE磁盘和SCSI磁盘。h代表IDE磁盘，s代表SCSI磁盘，故x取值为h和s。
 *      d表示disk, 即磁盘。
 *      y表示设备号，以区分第几个设备，取值范围是小写字符，其中a是第1个硬盘，b是第2个硬盘，依次类推。
 *      n表示分区号，也就是一个硬盘上的第几个分区。分区以数字1开始，依次类推。
 *
 * 综上所述：
 *      sda表示第1个SCSI硬盘，hdc表示第3个IDE硬盘，
 *      sda1表示第1个SCSI硬盘的第个分区，hdc3表示第3个IDE硬盘的第3个分区。
 *      这里统一用SCSI硬盘的命名规则来命名虚拟硬盘。
 */
void ide_init() {
    hd_count_init();
    // 初始化primary通道和secondary通道
    for (int cno = 0; cno < channel_cnt; ++cno) {
        ide_channel_t *channel = &channels[cno];
        // 1为次通道, 0为主通道
        strcpy(channel->name, cno ? "ide1" : "ide0");
        // 次通道寄存器端口基址0x170, 主通道0x1f0
        channel->base_port = cno ? 0x170 : 0x1f0;
        // 次通道走8259A从片最后一个中断引脚, 主通道走8259A从片倒数第二个中断引脚
        channel->int_no = 0x20 + (cno ? 15 : 14);
        channel->waiting = false;
        lock_init(&channel->lock);
        // 信号量初始为0，即硬盘驱动发送硬盘操作指令后，获取信号量即进入阻塞，直到硬盘完成后发送中断给一个信号量将其唤醒
        sema_init(&channel->disk_done, 0);

        // 初始化该通道上的硬盘
        uint8 cnt = hd_cnt <= 2 ? hd_cnt : (cno ? hd_cnt - 2 : 2); // 本通道上的硬盘数

        for (uint8 dno = 0; dno < cnt; ++dno) {
            disk_t *disk = &channel->disks[dno];
            // 0为主盘(master)，1为从盘(slave)
            disk->no = dno;
            disk->channel = channel;
            // 次通道: 从盘为sdd, 主盘为sdc， 主通道: 从盘为sdb, 主盘为sda
            strcpy(disk->name, cno ? (dno ? "sdd" : "sdc") : (dno ? "sdb" : "sda"));

            // 获取硬盘参数及分区信息


        }
    }
}

//
//    /*分别获取两个硬盘的参数及分区信息*/
//    while(dev_no<2){
//        struct disk_t* hd = &channel->devices[dev_no];
//        hd->channel=channel;
//        hd->dev_no=dev_no;
//        sprintf(hd->name,"sd%c",'a'+channelogic_no*2+dev_no);
//        identify_disk(hd);
////获取硬盘参数
//        if(dev_no!=0){//内核本身的裸硬盘（hd60M.img)不处理
//            partition_scan(hd,0);//扫描该硬盘上的分区
//        }
//        primary_no=0,1_no=0;
//        dev_no++;
//    }
//    dev_no=0;
////将硬盘驱动器号置0,为下一个channe1的两个硬盘初始化
//    channelogic_no++;
//下一个channel
/*处理每个通道上的硬盘*/
//while (channelogic_no < channel_cnt) {
//register_handler(channel
//->irq_no, intr_hd_handler);
//channelogic_no++; //下一个channel
//}
//
//}



/*
 * 在发送cmd指令之前执行，设置要操作的扇区：
 *    lba: 起始扇区
 *    sec_cnt: 扇区数，最多支持256个, 0表示256
 */
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
    outb(reg_dev(channel), (disk->no ? 0b11110000 : 0b11100000) | lba >> 24);
}


/*
 * 向通道发cmd命令:
 *    cmd: CMD_READ_SECTOR  读扇区
 *    cmd: CMD_WRITE_SECTOR 写扇区
 *    cmd: CMD_IDENTIFY     检测扇区
 */
static void cmd_out(ide_channel_t *channel, uint8 cmd) {
    channel->waiting = true;
    outb(reg_cmd(channel), cmd);
}

/*
 * 在硬盘中断发生后调用，即硬盘告知数据准备好所需的数据了，可以开始读了，
 * 将硬盘中的数据读到内存中来：
 *      buf: 读到哪里
 *      sec_cnt: 读多少个扇区，最多支持256个, 0表示256
 */
static void read_sector(disk_t *disk, void *buf, uint8 sec_cnt) {
    insw(reg_data(disk->channel), buf, (sec_cnt ? sec_cnt : 256) << 9 >> 1);  // 1扇区512字节, 按字读入
}

/*
 * 在硬盘中断发生后调用，即硬盘告知数据准备好接收数据了，可以开始发送了，
 * 将内存中的数据写入硬盘：
 *      buf: 从哪里读数据
 *      sec_cnt: 要读多少个扇区写入硬盘，最多支持256个, 0表示256
 */
static void write_sector(disk_t *disk, void *buf, uint8 sec_cnt) {
    outsw(reg_data(disk->channel), buf, (sec_cnt ? sec_cnt : 256) << 9 >> 1);  // 1扇区512字节, 按字写出
}

/*
 * 等待硬盘响应，用在发送完cmd命令之后调用。
 * 在ata手册中有这么一句话：“All actions required in this state shall be completed within 31s",
 * 大概意思是所有的操作都应该在31秒内完成，所以等待硬盘响应还是有个时间上限的，
 * 这里尝试读取硬盘状态，若成功则返回true
 */
static bool busy_wait(disk_t *disk) {
    // 我们在30秒内
    ide_channel_t *channel = disk->channel;

    // 这里将5秒转成时钟中断周期，如果还没获取到，就失败
    uint32 times = S2TIMES(5); // 注：这里的实际时间会和当前任务数有关
    for (int i = 0; i < times; ++i) {
        HLT
        if (inb(reg_status(channel)) & BIT_ALT_STAT_BSY) continue; // 读取状态位
        // 读取硬盘状态寄存器成功相当于告知CPU此次中断已被处理，即CPU需要赶紧将数据取走
        return inb(reg_status(channel)) & BIT_ALT_STAT_DRQ;
    }
    return false;
}


/*
 * 发送读盘指令，从硬盘读取指定扇区个数据到内存中：
 *      lba: 从哪个扇区开始读
 *      sec_cnt: 读多少个扇区
 *      buf: 读到哪里
 */
void ide_read(struct disk_t *disk, uint32 lba, void *buf, uint32 sec_cnt) {
    // 中断是对于通道而言的，这里要保证多任务之前不抢占通道
    lock(&disk->channel->lock);

    // secs_op为每次操作的扇区数, secs_done为已完成的扇区数
    for (uint32 secs_op, secs_done = 0; secs_done < sec_cnt; secs_done += secs_op) {
        // 计算本次操作的扇区数（每次最多读256个扇区），设置要读取的扇区
        secs_op = secs_done + 256 <= sec_cnt ? 256 : sec_cnt - secs_done;
        select_sector(disk, lba + secs_done, secs_op);
        // 发送读扇区命令
        cmd_out(disk->channel, CMD_READ_SECTOR);

        // 等待硬盘中断信号
        sema_down(&disk->channel->disk_done);

        // 已经等到硬盘唤醒信号，开始检测硬盘状态
        if (!busy_wait(disk)) {
            printk("[%s] disk busy!\n", __FUNCTION__);
            STOP
        }

        // 从硬盘缓冲区中将数据读出
        read_sector(disk, (void *) ((uint32) buf + secs_done * 512), secs_op);
    }

    // 释放通道锁
    unlock(&disk->channel->lock);
}

/*
 * 发送写盘指令，将指定数据写入硬盘：
 *      lba: 从哪个扇区开始写
 *      sec_cnt: 写多少个扇区
 *      buf: 从哪里读数据
 */
void ide_write(struct disk_t *disk, uint32 lba, void *buf, uint32 sec_cnt) {
    lock(&disk->channel->lock);

    for (uint32 secs_op, secs_done = 0; secs_done < sec_cnt; secs_done += secs_op) {
        secs_op = secs_done + 256 <= sec_cnt ? 256 : sec_cnt - secs_done;
        select_sector(disk, lba + secs_done, secs_op);
        // 发送写扇区命令
        cmd_out(disk->channel, CMD_WRITE_SECTOR);

        // 开始检测硬盘状态
        if (!busy_wait(disk)) {
            printk("[%s] disk busy!\n", __FUNCTION__);
            STOP
        }

        // 开始写盘
        write_sector(disk, (void *) ((uint32) buf + secs_done * 512), secs_op);

        // 等待硬盘中断信号
        sema_down(&disk->channel->disk_done);
        // 可以继续操作了
        secs_done += secs_op;
    }

    unlock(&disk->channel->lock);
}


/*
 * 将dst中len个相邻字节交换位置后存入buf,
 * 此函数用来处理identify命令的返回信息，硬盘参数信息是以字为单位的，包括偏移、长度的单位都是字，
 * 在这16位的字中，相邻字符的位置是互换的，所以通过此函数做转换。
 */
static void swap_pairs_bytes(const char *dst, char *buf, uint32 len) {
    uint32 idx;
    for (idx = 0; idx < len; idx += 2) {
        /*buf中存储dst中两相邻元素交换位置后的字符串*/
        buf[idx + 1] = *dst++;
        buf[idx] = *dst++;
    }
    buf[idx] = '\0';
}


/*
 * 获取硬盘参数信息：
 * ------------------------------------------------------------------------------------
 * identify命令获得的返回信息（部分）：
 *      ------------------------------------------
 *      | 字节偏移 |     描述                      |
 *      ------------------------------------------
 *      | 10~19  | 硬盘序列号，长度为20的字符串       |
 *      | 27~46  | 硬盘型号，长度为40的字符串        |
 *      | 60~61  | 可供用户使用的扇区数，长度为2的整型 |
 *      ------------------------------------------
 *      注意，该命令返回的结果以字为单位，而非字节
 * ------------------------------------------------------------------------------------
 */
static void identify_disk(disk_t *disk) {
    // 先选择要操作的盘
    // device寄存器, 参考 loader.asm 中 Device寄存器结构
    // 第5和7位固定为1，第6位MOD=1(LBA), 第4位DEV=0/1(主盘/从盘), 第0~3位为lba地址的24~27位
    outb(reg_dev(disk->channel), (disk->no ? 0b11110000 : 0b11100000));
    // 发送硬盘检测指令
    cmd_out(disk->channel, CMD_IDENTIFY);

    // 等待信号量
    sema_down(&disk->channel->disk_done);

    // 已等到信号
    if (!busy_wait(disk)) {
        printk("[%s] disk busy!\n", __FUNCTION__);
        STOP
    }

    // 读取第一个扇区
    char id_info[512];
    read_sector(disk, id_info, 1);

    char buf[64];
    // 读取硬盘序列号
    swap_pairs_bytes(&id_info[10 * 2], buf, 20);
    printk("disk %s info:\n SN:%s\n", disk->name, buf);
    memset(buf, 0, sizeof(buf));
    // 读取硬盘型号
    swap_pairs_bytes(&id_info[27 * 2], buf, 40);
    printk("MODULE:%s\n", buf);
    // 读取可用扇区数
    uint32 sectors = *(uint32 *) &id_info[60 * 2];
    printk("    SECTORS:%d\n", sectors);
    printk("    CAPACITY:%dMB\n", sectors * 512 / 1024 / 1024);
}

uint32 ext_lba_base = 0;            // 用于记录总扩展分区的起始lba, 初始为0, partition_scan时以此为标记
uint8 primary_no = 0, logic_no = 0; // 用来记录硬盘主分区和逻辑分区的下标
chain_t partition_list;             // 分区队列

/*
 * 扫描硬盘disk中地址为ext_lba的扇区中的所有分区
 */
static void partition_scan(disk_t *disk, uint32 ext_lba) {

    // 读取引导扇区
    boot_sector_t *bs = kmalloc(sizeof(boot_sector_t));
    ide_read(disk, ext_lba, bs, 1);

    partition_table_entry_t *p = bs->tables;

    // 读取分区表的4个分区表项
    for (uint8 part_idx = 0; part_idx < 4; ++part_idx) {
        // 如果不是有效分区类型，直接读下一个分区
        if (!p->fs_type) {
            p++;
            continue;
        }

        // 如果是非扩展分区
        if (p->fs_type != 0x5) {
            // 如果还没读到扩展分区，那一定是主分区
            if (ext_lba) {
                disk->prim_parts[primary_no].start_lba = p->start_lba;
                disk->prim_parts[primary_no].sec_cnt = p->sec_cnt;
                disk->prim_parts[primary_no].disk = disk;
                chain_put_last(&partition_list, &disk->prim_parts[primary_no].part_tag);
                primary_no++;
                p++;
                continue;
            }

            // 不是主分区，那就是逻辑分区
            disk->logic_parts[logic_no].start_lba = ext_lba + p->start_lba;
            disk->logic_parts[logic_no].sec_cnt = p->sec_cnt;
            disk->logic_parts[logic_no].disk = disk;
            chain_put_last(&partition_list, &disk->logic_parts[logic_no].part_tag);

            // 逻辑分区数字从5开始，主分区是1~4
            sprintfk(disk->logic_parts[logic_no].name, "%s%d", disk->name, logic_no + 5);
            logic_no++;

            p++;
            continue;
        }

        /* 扩展分区 */

        // 如果是首次读取到扩展分区, 即MBR所在的扇区
        if (!ext_lba_base) {
            // 记录扩展分区的起始lba地址, 后面所有的扩展分区地址都相对于此
            ext_lba_base = p->start_lba;
            // 扫描这块盘的分区信息
            partition_scan(disk, p->start_lba);

            p++;
            continue;
        }

        // 非首次读取扩展扇区, 即EBR所在的扇区
        partition_scan(disk, p->start_lba + ext_lba_base);
        p++;
    }
    kmfree(bs);
}

//
///*打印分区信息*/
//static bool partition_info( chain_elem_t *pelem,int arg UNUSED) {
//    struct partition *part = elem2entry(
//    struct partition, part_tag, pelem);
//    printk("%s start_lba:0x%x,sec_cnt:0x%x\n", \
//part->name, part->start_lba, part->sec_cnt);
///*在此处return false与函数本身功能无关，
//*只是为了让主调函数list_traversal继续向下遍历元素*/
//    return false;
//}



/*
 * 硬盘中断处理程序
 */
void hd_interrupt_handler(uint8 vector_no) {
    // 每次读写硬盘时会申请锁, 保证了同步一致性
    uint8 cno = vector_no - 0x2e; // 主通道终端号0x2f, 从通道中断号0x2e
    ide_channel_t *channel = &channels[cno];

    if (!channel->waiting) return;
    channel->waiting = false;

    // 唤醒任务
    sema_up(&channel->disk_done);

    // 读取硬盘状态寄存器，其便会认为此次的中断已被处理
    // inb(reg_status(channel));
}


disk_t *get_disk(uint8 channel_no, uint8 disk_no) {
    return &channels[channel_no].disks[disk_no];
}


// =======================================================
// ===============以下用于测试===========================
// =======================================================

//#define TEST_READ_SEC_CNT 4
//char buff[TEST_READ_SEC_CNT << 9] = {0};
//
//void test_read_disk() {
//    ide_read(&channels[0].disks[0], 0, buff, TEST_READ_SEC_CNT);
////    select_sector(&channels[0].disks[0], 0, TEST_READ_SEC_CNT);
////    cmd_out(&channels[0], CMD_READ_SECTOR);
//}
//
//
//void hd_interrupt_handler1() {
//    read_sector(&channels[0].disks[0], buff, TEST_READ_SEC_CNT);
//    uint32 size = TEST_READ_SEC_CNT << 9;
//    for (int i = 0; i < size; ++i) {
//        if (i % 16 == 0) printk("\n");
//        printk("%02x ", buff[i] & 0xff);
//    }
//    printk("\nhd_interrupt_handler end~ \n");
//}
//
