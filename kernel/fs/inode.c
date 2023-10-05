//
// Created by toney on 10/4/23.
//

#include "../../include/ide.h"
#include "../../include/string.h"
#include "../../include/eflags.h"
#include "../../include/print.h"
#include "../../include/file.h"

/**
 * 获取inode所在的扇区和扇区内的偏移量
 * @param part 分区
 * @param inode_no inode编号
 * @param inode_pos 用于填充数据的结构
 */
static void inode_locate(partition_t *part, uint32 inode_no, inode_position_t *inode_pos) {
    uint32 inode_size = sizeof(inode_t);
    uint32 off_size = inode_no * inode_size; // 总字节偏移量
    uint32 off_sec = off_size >> 9;          // 扇区偏移量
    uint32 off_size_in_sec = off_size % 512; // 扇区内的字节偏移量

    inode_pos->two_sec = 512 - off_size_in_sec < inode_size; // 此i结点是否跨越2个扇区
    inode_pos->sec_lba = part->sb->inode_table_lba + off_sec; // inode_table 在硬盘上是连续的
    inode_pos->off_size = off_size_in_sec;
}

/**
 * 将inode写入到分区
 * @param part 分区
 * @param inode 要写的i节点
 * @param io_buf 用于硬盘io的缓冲区
 */
void inode_sync(partition_t *part, inode_t *inode, void *io_buf) {
    // 定位inode位置
    inode_position_t inode_pos;
    inode_locate(part, inode->i_no, &inode_pos);

    //准备一个干净的inode结构
    inode_t pure_inode;
    memcpy(&pure_inode, inode, sizeof(inode_t));
    // 只保留关键信息, 将需不要的信息清除
    pure_inode.i_open_cnts = 0;
    pure_inode.write_deny = false;
    chain_elem_clear(&pure_inode.inode_tag);

    // 读写硬盘是以扇区为单位，所以需要将原硬盘上的内容先读出来再和新数据拼起来一起写入
    uint8 sec_cnt = inode_pos.two_sec ? 2 : 1; // 如果跨扇区则需要操作两个扇区的读写
    ide_read(part->disk, inode_pos.sec_lba, io_buf, sec_cnt);
    memcpy(((char *) io_buf + inode_pos.off_size), &pure_inode, sizeof(inode_t));
    ide_write(part->disk, inode_pos.sec_lba, io_buf, sec_cnt);

    printk("[%s] [%d] save to [0x%x: 0x%x]!\n", __FUNCTION__, inode->i_no, inode_pos.sec_lba << 9, inode_pos.off_size);
}

/**
 * 根据i结点号返回相应的i结点
 * @param part 分区
 * @param inode_no i节点编号
 * @return i结点
 */
inode_t *inode_open(partition_t *part, uint32 inode_no) {
    // 先从已打开的inode链表中找
    inode_t *inode_found;
    if (part->open_inodes.size) {
        for (chain_elem_t *elem = chain_read_first(&part->open_inodes); elem; elem = chain_read_next(&part->open_inodes,
                                                                                                     elem)) {
            inode_found = (inode_t *) ((char *) elem - (uint32) &((inode_t *) NULL)->inode_tag);
            if (inode_no == inode_found->i_no) {
                inode_found->i_open_cnts++;
                return inode_found;
            }
        }
    }

    // 已打开的inode链表中未找到，从硬盘上读入此inode并加入到此链表
    inode_position_t inode_pos;
    inode_locate(part, inode_no, &inode_pos); // 先定位

    // 从硬盘读出inode
    uint32 buf_size = inode_pos.two_sec ? 1024 : 512;
    char *inode_buf = (char *) kmalloc(buf_size);
    ide_read(part->disk, inode_pos.sec_lba, inode_buf, inode_pos.two_sec ? 2 : 1);

    // 复制, 添加到inode链表队首
    inode_found = (inode_t *) kmalloc(sizeof(inode_t));
    memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(inode_t));
    chain_put_first(&part->open_inodes, &inode_found->inode_tag);
    inode_found->i_open_cnts = 1; // 首次打开

    kmfree_s(inode_buf, buf_size);
    return inode_found;
}

/**
 * 关闭inode或减少inode的打开数
 * 若没有进程再打开此文件，将此inode去掉并释放空间
 * @param part 分区
 * @param inode 要关闭的i节点
 */
void inode_close(partition_t *part, inode_t *inode) {
    bool iflag = check_close_if();
    if (--inode->i_open_cnts == 0) {
        // 从已打开的inode链表中移除
        chain_remove(&part->open_inodes, &inode->inode_tag);    // 将I结点从part->open_inodes中去掉
        // 释放内存
        kmfree_s(inode, sizeof(inode_t));
    }
    check_recover_if(iflag);
}

/**
 * 初始化new_inode
 * @param inode_no i节点编号
 * @param new_inode 要初始化的inode
 */
void inode_init(uint32 inode_no, inode_t *new_inode) {
    new_inode->i_no = inode_no;
    new_inode->i_size = 0;
    for (int i = 0; i < 12; ++i) new_inode->direct_blocks[i] = 0;
    new_inode->indirect_block = 0;
    new_inode->i_open_cnts = 0;
    new_inode->write_deny = false;
}


/**
 * 将硬盘上的inode清空 (把对应硬盘位置写0, 从管理上来说不清零也可以, 只是硬盘上会留脏数据, 后面谁用再直接覆盖即可)
 * @param part inode所在分区
 * @param inode_no inode编号
 * @param io_buf 临时缓冲区
 */
static void inode_delete(partition_t *part, uint32 inode_no, void *io_buf) {
    // 将inode表中的该inode数据清空
    inode_position_t inode_pos;
    inode_locate(part, inode_no, &inode_pos); // inode位置信息会存入inode_pos

    char *inode_buf = (char *) io_buf;
    uint8 sec_cnt = inode_pos.two_sec ? 2 : 1;
    // 将原硬盘上的内容先读出来
    ide_read(part->disk, inode_pos.sec_lba, inode_buf, sec_cnt);
    // 将inode所在为位置清0
    memset((inode_buf + inode_pos.off_size), 0, sizeof(inode_t));
    // 用清0的内存数据覆盖磁盘
    ide_write(part->disk, inode_pos.sec_lba, inode_buf, sec_cnt);
}

// 回收inode的数据块和inode本身
void inode_release(partition_t *part, uint32 inode_no) {
    // 1, 获取到inode资源
    inode_t *inode_to_del = inode_open(part, inode_no);

    // 2，检查清空140个数据块
    uint8 check_block_cnt = inode_to_del->indirect_block ? 128 + 12 : 12;
    uint32 all_block_bytes = check_block_cnt << 2;
    uint32 *all_blocks = (uint32 *) kmalloc(all_block_bytes);
    memset(all_blocks, 0, all_block_bytes);

    // 先把直接块同步
    for (uint8 block_idx = 0; block_idx < 12; ++block_idx) {
        all_blocks[block_idx] = inode_to_del->direct_blocks[block_idx];
    }

    // 再把间接块同步
    if (inode_to_del->indirect_block) {
        ide_read(part->disk, inode_to_del->indirect_block, all_blocks + 12, 1);
        // 先回收间接表所在的块地址, 并同步块位图
        block_bitmap_free(part, inode_to_del->indirect_block);
        bitmap_sync(part, inode_to_del->indirect_block - part->sb->data_start_lba, BLOCK_BITMAP);
    }

    // 释放所有块地址（不考虑目录项的情况）
    for (uint8 block_idx = 0; block_idx < check_block_cnt; ++block_idx) {
        if (!all_blocks[block_idx]) continue; // 没有就不管
        // 释放块地址, 同步块位图
        block_bitmap_free(part, all_blocks[block_idx]);
        bitmap_sync(part, all_blocks[block_idx] - part->sb->data_start_lba, BLOCK_BITMAP);
    }

    // 2, 释放inode所在块地址，并回收位图, 清空inode
    inode_bitmap_free(part, inode_no);
    bitmap_sync(part, inode_no, INODE_BITMAP);

    // 这里为了调试，对其清空，如果不清空后面谁用谁覆盖也可以（像上面的间接表的block和inode块数据就没有清空）
    void *io_buf = kmalloc(1024);
    inode_delete(part, inode_no, io_buf);
    kmfree_s(io_buf, 1024);

    // 3，关闭inode
    inode_close(part, inode_to_del);
}