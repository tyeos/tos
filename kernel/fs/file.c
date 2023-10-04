//
// Created by toney on 10/4/23.
//


#include "../../include/ide.h"
#include "../../include/print.h"
#include "../../include/string.h"
#include "../../include/eflags.h"
#include "../../include/inode.h"
#include "../../include/dir.h"


extern task_t *current_task;
extern partition_t *cur_part;

// 文件表
file_t file_table[MAX_FILE_OPEN];

/**
 * 从文件表file_table中获取一个空闲位
 * @return 成功返回下标，失败返回 ERR_IDX
 */
uint32 get_free_slot_in_global() {
    // 跨过stdin, stdout, stderr
    for (uint32 fd_idx = 3; fd_idx < MAX_FILE_OPEN; fd_idx++) {
        if (file_table[fd_idx].fd_inode == NULL) return fd_idx;
    }
    printk("[%s] exceed max open files\n", __FUNCTION__);
    STOP
    return ERR_IDX;
}

/**
 * 将全局描述符下标安装到进程或线程自己的文件描述符数组fd_table中
 * @param global_fd_idx 全局描述符下标
 * @return 成功返回下标，失败返回-1
 */
int32 pcb_fd_install(int32 global_fd_idx) {
    // 跨过stdin, stdout, stderr
    for (uint8 local_fd_idx = 3; local_fd_idx < MAX_FILES_OPEN_PER_PROC; local_fd_idx++) {
        if (current_task->fd_table[local_fd_idx] == -1) { // -1表示free_slot,可用
            current_task->fd_table[local_fd_idx] = global_fd_idx;
            return local_fd_idx;
        }
    }
    printk("[%s] exceed max open files_per_proc\n", __FUNCTION__);
    return -1;
}

/**
 * 分配一个i结点
 * @param part 分区
 * @return i结点号
 */
uint32 inode_bitmap_alloc(partition_t *part) {
    uint32 bit_idx = bitmap_alloc(&part->inode_bitmap);
    return bit_idx;
}

/**
 * 分配1个扇区
 * @param part 分区
 * @return 扇区地址
 */
uint32 block_bitmap_alloc(partition_t *part) {
    uint32 bit_idx = bitmap_alloc(&part->block_bitmap);
    // 和inode_bitmap_malloc不同，此处返回的不是位图索引，而是具体可用的扇区地址
    return part->sb->data_start_lba + bit_idx;
}

/**
 * 将内存中bitmap第bit_idx位所在的512字节同步到硬盘
 * @param part 分区
 * @param bit_idx 同步bit的在位图中的位置
 * @param btmp 同步的位图类型
 */
void bitmap_sync(partition_t *part, uint32 bit_idx, enum bitmap_type btmp) {
    uint32 off_sec = bit_idx >> 12; // bit所在的 扇区 LBA偏移量
    uint32 off_size = off_sec << 9; // 该扇区 在位图中的 字节偏移量

    uint32 sec_lba;
    uint8 *bitmap_off;

    // 需要被同步到硬盘的位图只有inode_bitmap和block_bitmap
    switch (btmp) {
        case INODE_BITMAP:
            sec_lba = part->sb->inode_bitmap_lba + off_sec;
            bitmap_off = part->inode_bitmap.map + off_size;
            break;
        case BLOCK_BITMAP:
            sec_lba = part->sb->block_bitmap_lba + off_sec;
            bitmap_off = part->block_bitmap.map + off_size;
            break;
    }
    ide_write(part->disk, sec_lba, bitmap_off, 1);
}


/**
 * 创建文件
 * @param parent_dir 文件所属父目录
 * @param filename 文件名
 * @param flag 枚举, oflags
 * @return 成功则返回文件描述符，否则返回-1
 */
int32 file_create(dir_t *parent_dir, char *filename) {

    // 为新文件分配inode
    int32 inode_no = inode_bitmap_alloc(cur_part);
    inode_t *new_file_inode = (inode_t *) kmalloc(sizeof(inode_t));
    inode_init(inode_no, new_file_inode); // 初始化i结点

    // 返回的是file_table数组的下标
    int fd_idx = get_free_slot_in_global();
    file_table[fd_idx].fd_inode = new_file_inode;
    file_table[fd_idx].fd_pos = 0;
    file_table[fd_idx].fd_flag = O_CREAT;
    file_table[fd_idx].fd_inode->write_deny = false;

    // 创建目录项
    dir_entry_t new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(dir_entry_t));
    create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry);

    /* 同步内存数据到硬盘 */

    // 后续操作的公共缓冲区
    void *io_buf = kmalloc(1024);

    // a 在目录parent_dir下安装目录项new_dir_entry, 写入硬盘后返回true,否则false
    sync_dir_entry(parent_dir, &new_dir_entry, io_buf);

    // b 将父目录i结点的内容同步到硬盘
    inode_sync(cur_part, parent_dir->inode, io_buf);

    // c 将新创建文件的i结点内容同步到硬盘
    memset(io_buf, 0, 1024);
    inode_sync(cur_part, new_file_inode, io_buf);

    // d 将inode_bitmap位图同步到硬盘
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);

    // e 将创建的文件i结点添加到open_inodes链表
    chain_put_last(&cur_part->open_inodes, &new_file_inode->inode_tag);
    new_file_inode->i_open_cnts = 1;

    kmfree_s(io_buf, 1024);

    return pcb_fd_install(fd_idx);
}