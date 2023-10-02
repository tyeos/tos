//
// Created by toney on 10/2/23.
//

#include "../../include/fs.h"
#include "../../include/ide.h"
#include "../../include/print.h"
#include "../../include/string.h"
#include "../../include/sys.h"


/*
 * 格式化分区，也就是初始化分区的元信息，创建文件系统
 * ------------------------------------------------
 * 任意一个分区的结构：
 *      ----------------
 *      | 操作系统引导块  | (<-低地址)
 *      | 超级块         |
 *      | 空闲块位图     |
 *      | inode位图     |
 *      | inode数组     |
 *      | 根目录        |
 *      | 空闲块区域     | (<-高地址)
 *      ----------------
 * ------------------------------------------------
 */
static void partition_format(partition_t *part) {
    uint32 boot_sector_sects = 1;   // MBR占一个扇区
    uint32 super_block_sects = 1;   // 超级块占一个扇区
    uint32 inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);              // inode位图目前占1个扇区数
    uint32 inode_table_sects = DIV_ROUND_UP(sizeof(inode_t) * MAX_FILES_PER_PART, SECTOR_SIZE); // 达到文件创建上限的占用扇区数

    uint32 used_sects = boot_sector_sects + super_block_sects + inode_bitmap_sects + inode_table_sects; // 目前已使用的扇区
    uint32 free_sects = part->sec_cnt - used_sects;          // 目前空闲的扇区
    // 有种情况会使实际使用扇区比总扇区数少1个，即空闲块位图的最后一bit管理的刚好是倒数第二个空闲扇区，若再多管理一个空闲扇区就得给空闲块位图再分配一个扇区，所以最后一个扇区只能放弃不用
    free_sects -= DIV_ROUND_UP(free_sects, BITS_PER_SECTOR); // 更新: 目前空闲的扇区, 去掉闲块位图占的空间再计算一次
    uint32 block_bitmap_sects = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR); // 管理空闲块的位图需要占用的扇区数（先算缩减的空闲扇区）

    // 超级块初始化
    super_block_t sb;
    sb.magic = FS_TYPE_MAGIC;
    sb.sec_cnt = part->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;
    sb.part_lba_base = part->start_lba;

    sb.block_bitmap_lba = sb.part_lba_base + 2; // 第0块是引导块，第1块是超级块，之后是空闲块位图
    sb.block_bitmap_sects = block_bitmap_sects;

    sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
    sb.inode_bitmap_sects = inode_bitmap_sects;

    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;
    sb.inode_table_sects = inode_table_sects;

    sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;
    sb.data_sects = free_sects;
    sb.root_inode_no = 0;
    sb.dir_entry_size = sizeof(dir_entry_t);

    printk("--------------\n");
    printk("[%s] %s info:\n", __FUNCTION__, part->name);
    printk("             magic :%d\n", sb.magic);
    printk("           sec_cnt :%d\n", sb.sec_cnt);
    printk("         inode_cnt :%d\n", sb.inode_cnt);
    printk("     part_lba_base :%d\n", sb.part_lba_base);
    printk("  block_bitmap_lba :%d\n", sb.block_bitmap_lba);
    printk("block_bitmap_sects :%d\n", sb.block_bitmap_sects);
    printk("  inode_bitmap_lba :%d\n", sb.inode_bitmap_lba);
    printk("inode_bitmap_sects :%d\n", sb.inode_bitmap_sects);
    printk("   inode_table_lba :%d\n", sb.inode_table_lba);
    printk(" inode_table_sects :%d\n", sb.inode_table_sects);
    printk("    data_start_lba :%d\n", sb.data_start_lba);
    printk("     root_inode_no :%d\n", sb.root_inode_no);
    printk("    dir_entry_size :%d\n", sb.dir_entry_size);


    // ---------- 将相关信息写入磁盘对应的分区 ----------
    disk_t *hd = part->disk;

    // ---------- 0扇区为引导扇区（占1个扇区） ----------

    // ---------- 1扇区为超级块（占1个扇区） ----------
    ide_write(hd, part->start_lba + 1, &sb, 1);
    printk("[%s] %s write super block: 0x%x\n", __FUNCTION__, part->name, (part->start_lba + 1) << 9);

    // ---------- 2扇区开始为空闲块位图（占多个扇区）----------

    /*
     * 1bit管理1扇区, 1个page的bit可以管理8<<12个扇区，
     * 1扇区512字节，即1个page可管理 1<<24字节 = 8MB 硬盘空间
     * 一般空闲空间都是8MB的很多倍（排除测试时分配小内存的情况）,
     * 除了空闲块位图，后面还需要给 inode位图、inode数组 初始化，也是同理，inode数组占用也很大，估计要几百个扇区，
     * 所以这里需要申请多页内存给空闲块位图做临时存储缓冲区, 所以这里按最大的申请一次临时共用即可。
     */
    uint32 max_sects = sb.block_bitmap_sects > sb.inode_table_sects ? sb.block_bitmap_sects : sb.inode_table_sects;
    max_sects = max_sects > sb.inode_bitmap_sects ? max_sects : sb.inode_bitmap_sects;
    uint32 alloc_pages = (max_sects + 7) >> 3; // 1个page = 8个扇区
    uint8 *buf = alloc_kernel_pages(alloc_pages);

    // 初始化块位图
    memset(buf, 0, sb.block_bitmap_sects << 9);
    buf[0] |= 0b1; // 第0个块预留给根目录，位图中先占位

//    uint32 block_bitmap_last_byte = free_sects / 8;
//    uint8 block_bitmap_last_bit = free_sects % 8;
//    uint32 last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE);
//    // last_size是位图所在最后一个扇区中，不足一扇区的其余部分
//    // 1 先将位图最后一字节到其所在的扇区的结束全置为1, 即超出实际块数的部分直接置为已占用
//    memset(&buf[block_bitmap_last_byte], 0xff, last_size);
//    // 2 再将上一步中覆盖的最后一字节内的有效位重新置0
//    uint8 bit_idx = 0;
//    while (bit_idx <= block_bitmap_last_bit) {
//        buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
//    }
    ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);
    printk("[%s] %s write block bitmap: 0x%x\n", __FUNCTION__, part->name, sb.block_bitmap_lba << 9);


    // ---------- inode位图（可占多个扇区，目前设计是占1个扇区）----------
    memset(buf, 0, sb.inode_bitmap_sects << 9);
    buf[0] |= 0b1; // 第0个inode分给根目录
    ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sects);
    printk("[%s] %s write inode bitmap: 0x%x\n", __FUNCTION__, part->name, sb.inode_bitmap_lba << 9);


    // ---------- inode数组（占多个扇区）----------
    memset(buf, 0, sb.inode_table_sects << 9);
    // 先写inode_table中的第0项，即根目录所在的inode
    inode_t *i = (inode_t *) buf;
    i->i_size = sb.dir_entry_size * 2;   // .和..
    i->i_no = 0;                         // 根目录占inode数组中第0个inode
    i->i_sectors[0] = sb.data_start_lba; // 指向空闲区起始位置，i_sectors数组的其他元素目前都为0
    ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);
    printk("[%s] %s write inode table: 0x%x\n", __FUNCTION__, part->name, sb.inode_table_lba << 9);


    // ---------- 根目录（占1个扇区）----------

    // 写入根目录的两个目录项.和..
    memset(buf, 0, SECTOR_SIZE);
    dir_entry_t *p_de = (dir_entry_t *) buf;

    // 初始化当前目录"."
    memcpy(p_de->filename, ".", 1);
    p_de->i_no = 0;
    p_de->f_type = FT_DIRECTORY;
    p_de++;

    // 初始化当前目录父目录".."
    memcpy(p_de->filename, "..", 2);
    p_de->i_no = 0; //根目录的父目录依然是根目录自己
    p_de->f_type = FT_DIRECTORY;

    // 根目录分配在空闲数据区的起始位置，这里保存根目录的目录项
    ide_write(hd, sb.data_start_lba, buf, 1);
    printk("[%s] %s write root dir: 0x%x\n", __FUNCTION__, part->name, sb.data_start_lba << 9);

    free_kernel_pages(buf, alloc_pages);
}

/*
 * 检查分区是否安装了文件系统，若未安装，则进行格式化分区，创建文件系统
 */
void file_sys_init(chain_t *partitions) {
    printk("[%s] searching %d partitions...\n", __FUNCTION__, partitions->size);
    if (!partitions->size) return;
    // sb_buf用来存储从硬盘上读入的超级块
    super_block_t *sb_buf = (super_block_t *) kmalloc(SECTOR_SIZE);
    partition_t *part;
    for (chain_elem_t *elem = chain_read_first(partitions); elem; elem = chain_read_next(partitions, elem)) {
        part = elem->value;
        memset(sb_buf, 0, SECTOR_SIZE);
        /* 读出分区的超级块，根据魔数是否正确来判断是否存在文件系统 */
        ide_read(part->disk, part->start_lba + 1, sb_buf, 1);
        /* 只支持自己的文件系统，若磁盘上已经有文件系统就不再格式化了 */
        if (sb_buf->magic == FS_TYPE_MAGIC) {
            printk("[%s] %s file system already exists ~\n", __FUNCTION__, part->name);
            continue;
        }
        // 其他文件系统不支持，一律按无文件系统处理
        printk("[%s] %s file system formatting ~\n", __FUNCTION__, part->name);
        partition_format(part);
    }
    kmfree_s(sb_buf, SECTOR_SIZE);
}