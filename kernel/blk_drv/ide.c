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

/*
 * reg_alt_status寄存器的一些关键位
 * 参考 loader.asm 中 Status寄存器结构
 */
#define BIT_ALT_STAT_BSY 0x80   // 硬盘忙
#define BIT_ALT_STAT_RDY 0x40   // 驱动器准备好了
#define BIT_ALT_STAT_DRQ 0x8    // 数据传输准备好了


#define PRIMARY_PARTITION_BASE_IDX 1 // 主分区号从1开始（具体看MBR分区表中的位置，1~4）
#define LOGIC_PARTITION_BASE_IDX 5   // 逻辑分区号从5开始依次递增

// 最多两个IDE通道
static ide_channel_t channels[2];
static uint8 channel_cnt = 0;   // 总通道数
static uint8 hd_cnt = 0;        // 总硬盘数

static chain_t partition_chain; // 分区队列


// 获取硬盘数量
static void hd_count_init() {
    hd_cnt = *((uint8 *) HD_NUMBER_MEMORY_PTR);
    // 这里简单计算，即硬盘按顺序挂载，前两块盘挂主通道，从第三块盘开始挂从通道
    channel_cnt = hd_cnt > 2 ? 2 : 1;
    printk("ide: hd_cnt = %d \n", hd_cnt);
}

/*
 * 等待硬盘响应。
 * 在ata手册中有这么一句话：“All actions required in this state shall be completed within 31s",
 * 大概意思是所有的操作都应该在31秒内完成，所以等待硬盘响应还是有个时间上限的，
 * 这里读取硬盘状态，如果繁忙则尝试等待，若成功则返回true
 *
 * 这里有两个调用时机：
 * 1，用在发送完cmd命令之后调用（flag = BIT_ALT_STAT_DRQ）
 *      一般情况下检测即可通过，这个是用在操作多扇区时，硬盘会频繁中断和修改状态。
 * 2，用在发送完cmd命令之前调用（flag = BIT_ALT_STAT_RDY）
 *      一般情况下硬盘都是一直准备好的状态，当然也不排除特殊情况，这里调用有两个作用：
 *      1.1，确保硬盘可用
 *      1.2，告知硬盘上一次的数据已经处理完毕（如果硬盘认为上一次的数据未处理，会一直阻塞）
 */
static bool busy_wait(ide_channel_t *channel, uint8 flag) {
    // 这里将5秒转成时钟中断周期，如果还没获取到，就失败
    uint32 times = S2TIMES(5); // 注：这里的实际时间会和当前任务数有关
    uint8 status;
    for (int i = 0; i < times; ++i) {
        status = inb(reg_status(channel));
//    printk("[%s] status = 0x%x!\n", __FUNCTION__, status & 0xff);
        if (!(status & BIT_ALT_STAT_BSY)) return status & BIT_ALT_STAT_DRQ;
        HLT
    }
    return false;
}

/*
 * 读取硬盘状态，看是否是非繁忙之外的某个标记状态（BIT_ALT_STAT_DRQ | BIT_ALT_STAT_RDY）。
 * 读取硬盘状态寄存器成功相当于告知CPU此次中断已被处理，即CPU需要赶紧将数据取走
 */
static bool check_status_not_busy(ide_channel_t *channel, uint8 flag) {
    uint8 status = inb(reg_status(channel));
//    printk("[%s] status = 0x%x!\n", __FUNCTION__, status & 0xff);
    if (!(status & BIT_ALT_STAT_BSY)) return status & flag;
    return false;
}

/*
 * 在发送cmd指令之前执行，设置要操作的扇区：
 *    lba: 起始扇区
 *    sec_cnt: 扇区数，最多支持256个, 0表示256
 *
 * 注：经测试，一般情况下，硬盘中断次数会和设置的扇区数相等，即中断次数为 1~256次，
 *      每次中断，可以多次检测硬盘状态，一般都为准备好数据（DRQ=1）的状态，
 *      读完一个扇区之后，再次检测硬盘状态，一般状态都会发生变更为未准备好数据（DRQ=0）的状态，
 *      即单次检测硬盘状态只保证接下来读取的一个扇区没问题，硬盘会持续准备后续扇区的数据，
 *      当下一个扇区数据准备好后，又会变更数据状态（DRQ=1），并重新发起硬盘中断。
 *      但是，如果只读了硬盘状态，但没有将数据读出，那么硬盘将不会再发起后续中断（相当于被阻塞）。
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
    // 发送命令之前，先检测硬盘状态，确保可用
    // 先立即检测一次，准备好可直接读，如果硬盘没准备好，则等待硬盘，直至超时
    if (!check_status_not_busy(channel, BIT_ALT_STAT_RDY) && !busy_wait(channel, BIT_ALT_STAT_RDY)) {
        printk("[%s] disk busy!\n", __FUNCTION__);
        STOP
    }
    channel->waiting = true;
    outb(reg_cmd(channel), cmd);
}

/*
 * 在硬盘中断发生后调用，即硬盘告知数据准备好所需的数据了，可以开始读了，
 * 将硬盘中的数据读到内存中来：
 *      buf: 读到哪里
 *      sec_cnt: 读多少个扇区，最多支持256个, 0表示256
 */
static void read_sector(ide_channel_t *channel, void *buf, uint8 sec_cnt) {
    // 读取数据之前，先检测硬盘状态，确保可用
    // 先立即检测一次，准备好可直接读，如果硬盘没准备好，则等待硬盘，直至超时
    if (!check_status_not_busy(channel, BIT_ALT_STAT_DRQ) && !busy_wait(channel, BIT_ALT_STAT_DRQ)) {
        printk("[%s] disk busy!\n", __FUNCTION__);
        STOP
    }
    insw(reg_data(channel), buf, (sec_cnt ? sec_cnt : 256) << 9 >> 1);  // 1扇区512字节, 按字读入
}

/*
 * 在硬盘中断发生后调用，即硬盘告知数据准备好接收数据了，可以开始发送了，
 * 将内存中的数据写入硬盘：
 *      buf: 从哪里读数据
 *      sec_cnt: 要读多少个扇区写入硬盘，最多支持256个, 0表示256
 */
static void write_sector(ide_channel_t *channel, void *buf, uint8 sec_cnt) {
    // 读取数据之前，先检测硬盘状态，确保可用
    // 先立即检测一次，准备好可直接写，如果硬盘没准备好，则等待硬盘，直至超时
    if (!check_status_not_busy(channel, BIT_ALT_STAT_DRQ) && !busy_wait(channel, BIT_ALT_STAT_DRQ)) {
        printk("[%s] disk busy!\n", __FUNCTION__);
        STOP
    }
    outsw(reg_data(channel), buf, (sec_cnt ? sec_cnt : 256) << 9 >> 1);  // 1扇区512字节, 按字写出
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

    /*
     * 由于多扇区读取一次请求会对应多次中断，所以这里有三种读取方案：
     *  1，每次读取一个扇区，即将请求和中断一一对应
     *  2，每次读取最多256个扇区，收到中断后，这里每读一个扇区，判断一次硬盘状态，硬盘就绪后再读
     *  3，将数据填充放到中断回调处，每次中断读取一个扇区，所有扇区读取完成后，再发送信号过来
     *
     * 三种方案皆可，这里为了结构简单，选择第一种方式。
     */
    for (int i = 0; i < sec_cnt; ++i) {
        // 内部一次读一个扇区
        select_sector(disk, lba + i, 1);
        // 发送读扇区命令
        cmd_out(disk->channel, CMD_READ_SECTOR);
        // 等待硬盘中断信号
        sema_down(&disk->channel->disk_done);

        // 已经等到硬盘唤醒信号，从硬盘缓冲区中将数据读出，一次读一个扇区（内部自动检测状态）
        read_sector(disk->channel, (void *) ((uint32) buf + (i << 9)), 1);
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
    /*
     * 逻辑和读扇区类似
     */
    for (int i = 0; i < sec_cnt; ++i) {
        // 内部一次写一个扇区
        select_sector(disk, lba + i, 1);

        // 发送写扇区命令
        cmd_out(disk->channel, CMD_WRITE_SECTOR);

        // 开始写盘（内部自动检测状态）
        write_sector(disk->channel, (void *) ((uint32) buf + (i << 9)), 1);

        // 数据写入完成，等待硬盘中断信号
        sema_down(&disk->channel->disk_done);

        // 收到中断需要读取一下状态，即告知硬盘数据响应已被处理，然后即可进行下一次写入
        check_status_not_busy(disk->channel, BIT_ALT_STAT_DRQ);
    }

    unlock(&disk->channel->lock);
}


/*
 * 将dst中len个相邻字节交换位置后存入buf,
 *      此函数用来处理identify命令的返回信息，硬盘参数信息是以字为单位的，包括偏移、长度的单位都是字，
 *      在这16位的字中，相邻字符的位置是互换的，所以通过此函数做转换。
 * 注：该函数会在最后补一个字符串结束符
 */
static void swap_pairs_bytes(const char *dst, char *buf, uint32 len) {
    uint32 idx;
    for (idx = 0; idx < len; idx += 2) {
        /*buf中存储dst中两相邻元素交换位置后的字符串*/
        buf[idx + 1] = *dst++;
        buf[idx] = *dst++;
    }
    buf[idx] = EOS;
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
    /*
     * device寄存器, 参考 loader.asm 中 Device寄存器结构
     * 第5和7位固定为1，第6位MOD=1(LBA), 第4位DEV=0/1(主盘/从盘), 第0~3位为lba地址的24~27位
     */
    outb(reg_dev(disk->channel), (disk->no ? 0b11110000 : 0b11100000));

    // 发送硬盘检测指令
    cmd_out(disk->channel, CMD_IDENTIFY);

    // 等待信号量
    sema_down(&disk->channel->disk_done);

    // 已等到信号，读取第一个扇区（内部自动检测状态）
    char *id_info = kmalloc(SECTOR_SIZE);
    read_sector(disk->channel, id_info, 1);

    // 读取硬盘序列号
    swap_pairs_bytes(&id_info[10 * 2], disk->sn, 20);
    // 读取硬盘型号
    swap_pairs_bytes(&id_info[27 * 2], disk->module, 40);
    // 读取可用扇区数
    disk->sec_cnt = *(uint32 *) &id_info[60 * 2];
    kmfree_s(id_info, SECTOR_SIZE);

    printk("\n[%s] disk %s info:\n", __FUNCTION__, disk->name);
    printk("    SN:       %s\n", disk->sn);
    printk("    MODULE:   %s\n", disk->module);
    printk("    SECTORS:  %d\n", disk->sec_cnt);
    printk("    CAPACITY: %dMB\n", disk->sec_cnt >> 11);
}

static uint8 whole_logic_idx = 0;    // 记录总的逻辑分区下标
/*
 * 扫描扩展分区，内部递归调用
 */
static void scan_ext_partitions(disk_t *disk, uint32 ext_lba_base) {
    printk("----------- ext partition --------------\n");
    printk("[%s] base: %d\n", __FUNCTION__, ext_lba_base);

    // 扫描这块盘的分区信息
    boot_sector_t *ebs = kmalloc(SECTOR_SIZE);
    ide_read(disk, ext_lba_base, ebs, 1);
    print_hex_buff((char *) ebs, SECTOR_SIZE);

    for (uint8 sub_logic_idx = 0; sub_logic_idx < 4; ++sub_logic_idx) {
        // 如果不是有效分区类型，直接读下一个分区
        if (!ebs->tables[sub_logic_idx].fs_type) {
            printk("[%s][%d] index %d not installed ~!\n", __FUNCTION__, ext_lba_base, sub_logic_idx);
            continue;
        }

        // 如果是非扩展分区, 那一定是逻辑分区
        if (ebs->tables[sub_logic_idx].fs_type != 0x5) {
            // 逻辑分区目前约定上限存4个，再多就不存了，打印信息即可
            if (whole_logic_idx > 3) {
                printk("[%s][%d] index %d is logic partition %s%d: \n", __FUNCTION__, ext_lba_base, sub_logic_idx,
                       disk->name, whole_logic_idx + LOGIC_PARTITION_BASE_IDX);
                printk("    start_lba: %d\n", ext_lba_base + ebs->tables[sub_logic_idx].start_lba);
                printk("    sec_cnt:   %d\n", ebs->tables[sub_logic_idx].sec_cnt);
                whole_logic_idx++;
                continue;
            }

            // 正常保存逻辑分区
            partition_t *partition = &disk->logic_parts[whole_logic_idx];
            partition->disk = disk;
            partition->start_lba = ext_lba_base + ebs->tables[sub_logic_idx].start_lba;
            partition->sec_cnt = ebs->tables[sub_logic_idx].sec_cnt;
            // 逻辑分区固定从5开始, 依次递增
            sprintfk(partition->name, "%s%d", disk->name, whole_logic_idx + LOGIC_PARTITION_BASE_IDX);
            whole_logic_idx++;

            printk("\n[%s][%d] index %d is logic partition %s:\n", __FUNCTION__, ext_lba_base, sub_logic_idx,
                   partition->name);
            printk("    start_lba: %d\n", partition->start_lba);
            printk("    sec_cnt:   %d\n", partition->sec_cnt);

            chain_elem_t *elem = kmalloc(sizeof(chain_elem_t));
            elem->value = partition;
            chain_put_last(&partition_chain, elem);
            continue;
        }

        // 子扩展分区，可以无限套娃，所以这里采用递归调用
        printk("[%s][%d] index %d is sub ext partition ~\n", __FUNCTION__, ext_lba_base, sub_logic_idx);
        scan_ext_partitions(disk, ext_lba_base + ebs->tables[sub_logic_idx].start_lba);
    }

    kmfree_s(ebs, SECTOR_SIZE);
}


/*
 * 扫描硬盘disk中地址为ext_lba的扇区中的所有分区
 */
static void scan_partitions(disk_t *disk) {

    // 读取MBR引导扇区
    boot_sector_t *mbs = kmalloc(SECTOR_SIZE);
    ide_read(disk, 0, mbs, 1);

    printk("-----------mbs--------------\n");
    print_hex_buff((char *) mbs, SECTOR_SIZE);

    for (uint8 primary_idx = 0; primary_idx < 4; ++primary_idx) {
        // 如果不是有效分区类型，直接读下一个分区
        if (!mbs->tables[primary_idx].fs_type) {
            printk("[%s] index %d not installed ~\n", __FUNCTION__, primary_idx);
            continue;
        }

        // 如果是非扩展分区, 那一定是主分区
        if (mbs->tables[primary_idx].fs_type != 0x5) {
            partition_t *partition = &disk->prim_parts[primary_idx];
            partition->disk = disk;
            partition->start_lba = mbs->tables[primary_idx].start_lba;
            partition->sec_cnt = mbs->tables[primary_idx].sec_cnt;
            sprintfk(partition->name, "%s%d", disk->name, primary_idx + PRIMARY_PARTITION_BASE_IDX);

            printk("\n[%s] index %d is primary partition %s:\n", __FUNCTION__, primary_idx, partition->name);
            printk("    start_lba: %d\n", partition->start_lba);
            printk("    sec_cnt:   %d\n", partition->sec_cnt);

            // 把分区加到链表中
            chain_elem_t *elem = kmalloc(sizeof(chain_elem_t));
            elem->value = partition;
            chain_put_last(&partition_chain, elem);
            continue;
        }

        /* 这里一定是总扩展分区 */
        printk("[%s] index %d is total ext partition ~\n", __FUNCTION__, primary_idx);

        // 传入扩展分区的起始lba地址, 后面所有的扩展分区地址都相对于此
        scan_ext_partitions(disk, mbs->tables[primary_idx].start_lba);
    }

    kmfree_s(mbs, SECTOR_SIZE);
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
    // 初始化硬盘数和通道数
    hd_count_init();

    // 初始化分区链表与缓存池
    chain_init(&partition_chain);

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
            // 主通道主从盘，次通道主从盘，依次为 sd[a~d]
            sprintfk(disk->name, "sd%c", 'a' + cno * 2 + dno);

            // 获取硬盘基本参数
            identify_disk(disk);

            // 获取硬盘分区信息（主通道的主盘作为操作系统盘，不设分区）
            if (cno != 0 || dno != 0) scan_partitions(disk);
        }
    }

    // 分区检测完毕，检查文件系统
//    file_sys_init(&partition_chain);
}

/*
 * 硬盘中断处理程序
 */
void hd_interrupt_handler(uint8 vector_no) {
//    printk("[%s] %d start ~\n", __FUNCTION__, vector_no);

    // 每次读写硬盘时会申请锁, 保证了同步一致性
    uint8 cno = vector_no - 0x2e; // 主通道终端号0x2f, 从通道中断号0x2e
    ide_channel_t *channel = &channels[cno];
    if (!channel->waiting) {
        printk("[%s] [%s] not fount waiter [0x%x] ~", __FUNCTION__, channel->name, vector_no);
        STOP
        return;
    }
    // 唤醒任务
    channel->waiting = false;
    sema_up(&channel->disk_done);

//    printk("[%s] %d end!\n", __FUNCTION__, vector_no);
}


disk_t *get_disk(uint8 channel_no, uint8 disk_no) {
    return &channels[channel_no].disks[disk_no];
}
