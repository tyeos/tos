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
 * 从文件表file_table中获取一个空闲槽位
 * @return 成功返回下标，失败返回 ERR_IDX
 */
int32 get_free_slot_in_global() {
    // 跨过stdin, stdout, stderr
    for (int32 fd_idx = 3; fd_idx < MAX_FILE_OPEN; fd_idx++) {
        if (file_table[fd_idx].fd_inode == NULL) return fd_idx;
    }
    printk("[%s] exceed max open files\n", __FUNCTION__);
    STOP
    return ERR_IDX;
}

/**
 * 将全局描述符下标安装到进程或线程自己的文件描述符数组fd_table中
 * @param global_fd_idx 全局描述符下标
 * @return 成功返回下标，失败返回 ERR_IDX
 */
int32 pcb_fd_install(int32 global_fd_idx) {
    // 跨过stdin, stdout, stderr
    for (uint8 local_fd_idx = 3; local_fd_idx < MAX_FILES_OPEN_PER_PROC; local_fd_idx++) {
        if (current_task->fd_table[local_fd_idx] == STD_FREE_SLOT) {
            current_task->fd_table[local_fd_idx] = global_fd_idx;
            return local_fd_idx;
        }
    }
    printk("[%s] exceed max open files_per_proc \n", __FUNCTION__);
    STOP
    return ERR_IDX;
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
    uint32 inode_no = inode_bitmap_alloc(cur_part);
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

/**
 * 打开指定编号的inode对应的文件
 * @param inode_no inode编号
 * @param flag 枚举 oflags 可以是组合值
 * @return 成功则返回文件描述符，否则返回 ERR_IDX
 */
int32 file_open(uint32 inode_no, uint8 flag) {
    int32 fd_idx = get_free_slot_in_global();
    if (fd_idx == (uint32) ERR_IDX) {
        printk("[%s] no free slots!\n", __FUNCTION__);
        STOP
        return ERR_IDX;
    }

    file_table[fd_idx].fd_inode = inode_open(cur_part, inode_no);
    file_table[fd_idx].fd_pos = 0;

    // 每次打开文件，要将fd_pos还原为0,即让文件内的指针指向开头
    file_table[fd_idx].fd_flag = flag;
    bool *write_deny = &file_table[fd_idx].fd_inode->write_deny;

    // 只要是关于写文件，判断是否有其他进程正写此文件, 若是读文件，不考虑write_deny
    if (flag & O_WRONLY || flag & O_RDWR) {
        // 以下进入临界区前先关中断
        bool iflag = check_close_if();
        if (*write_deny) { // 直接失败返回
            check_recover_if(iflag);
            printk("[%s] file can't be write now, try again later!\n", __FUNCTION__);
            STOP
            return ERR_IDX;
        }
        *write_deny = true;   // 置为true,避免多个进程同时写此文件
        check_recover_if(iflag);
    }

    // 若是读文件或创建文件，不用理会write_deny, 保持默认
    return pcb_fd_install(fd_idx);
}

// 把buf中的count个字节写入file, 成功则返回写入的字节数，失败则返回-1
int32 file_write(file_t *file, const void *buf, uint32 count) {
    // 文件目前最大只支持 512*140=71680 字节
    if (file->fd_inode->i_size + count > BLOCK_SIZE * 140) {
        printk("[%s] exceed max file size is %d bytes! \n", __FUNCTION__, BLOCK_SIZE * 140);
        return -1;
    }

    // 1，先要计算这次写入需要占用几个块，从哪个块的哪个地方开始写，写到哪个块的哪个地方结束

    uint32 start_byte_offset = file->fd_inode->i_size;                  // 之前的数据大小，也是这次从文件的哪个字节开始写
    uint8 start_block_offset = start_byte_offset >> 9;                  // 从第几个块开始写
    uint32 start_byte_offset_in_block = start_byte_offset % BLOCK_SIZE; // 从开始块的第几个字节开始写

    uint32 end_byte_offset = start_byte_offset + count - 1;             // 到文件的哪个字节处截止（包含该字节）
    uint8 end_block_offset = end_byte_offset >> 9;                      // 写到第几个块结束
    uint32 end_byte_offset_in_block = end_byte_offset % BLOCK_SIZE;     // 写到结束块的第几个字节处截止（包含该字节）

    // 2，将需要用的块指针对应的block创建出来并关联上

    // 用来记录文件所有的块地址（最多支持12+128=140个块）
    uint32 all_block_bytes = end_block_offset < 12 ? 48 : BLOCK_SIZE + 48;
    uint32 *all_blocks = (uint32 *) kmalloc(all_block_bytes);
    memset(all_blocks, 0, all_block_bytes);
    // 直接块中只把用到的同步就行
    for (uint8 block_idx = start_block_offset; block_idx < 12 && block_idx <= end_block_offset; ++block_idx) {
        all_blocks[block_idx] = file->fd_inode->direct_blocks[block_idx];
    }
    // 如果用到间接块，需要先保证间接块已创建，并保证同步
    if (end_block_offset >= 12) {
        if (file->fd_inode->indirect_block) { // 如果本来就有值，读进来就行
            ide_read(cur_part->disk, file->fd_inode->indirect_block, all_blocks + 12, 1);
        } else {
            // 创建间接块, 申请块地址, 同步块位图
            uint32 block_lba = block_bitmap_alloc(cur_part);
            bitmap_sync(cur_part, block_lba - cur_part->sb->data_start_lba, BLOCK_BITMAP);
            file->fd_inode->indirect_block = block_lba;
            // 这里暂不创建同步和子项，后面统一处理
        }
    }

    bool save_indirect_block = false;
    for (uint8 block_idx = start_block_offset; block_idx <= end_block_offset; ++block_idx) {
        // 这里保证所有的块都已经申请好
        if (all_blocks[block_idx]) continue; // 有值的不用管

        // 申请块地址, 同步块位图, 并同步到父目录inode内存
        uint32 block_lba = block_bitmap_alloc(cur_part);
        bitmap_sync(cur_part, block_lba - cur_part->sb->data_start_lba, BLOCK_BITMAP);
        all_blocks[block_idx] = block_lba;
        if (block_idx >= 12)save_indirect_block = true; // 只要间接块有新建，就需要同步到硬盘
        else file->fd_inode->direct_blocks[block_idx] = block_lba; // 直接块同步到inode中即可，最后统一落盘
    }
    // 如果新创建了间接块，那么将创建的间接块地址同步到硬盘
    if (save_indirect_block) ide_write(cur_part->disk, file->fd_inode->indirect_block, all_blocks + 12, 1);

    // 3，按块写数据即可，写完更新文件大小，并将inode同步到硬盘
    const uint8 *src = buf;                                  // 临时存储待写入的数据
    uint8 *io_buf = kmalloc(BLOCK_SIZE << 1);           // 临时缓冲区
    uint32 item_write_start, item_write_end, item_write_cnt; // 临时变量
    for (uint8 block_idx = start_block_offset; block_idx <= end_block_offset; ++block_idx) {
        memset(io_buf, 0, BLOCK_SIZE);
        item_write_start = block_idx == start_block_offset ? start_byte_offset_in_block : 0;
        item_write_end = block_idx == end_block_offset ? end_byte_offset_in_block + 1 : BLOCK_SIZE; // 这里不包含结尾字符

        // 如果是首个块，而且不是从0开始写，那需要先把原来的数据读出来，然后加入新数据一起写入
        if (item_write_start != 0) ide_read(cur_part->disk, all_blocks[block_idx], io_buf, 1);

        // 将数据写入block
        item_write_cnt = item_write_end - item_write_start; // 本次写入字节数
        memcpy(io_buf + item_write_start, src, item_write_cnt);
        ide_write(cur_part->disk, all_blocks[block_idx], io_buf, 1);
        src += item_write_cnt;

        printk("[%s] to 0x%x: [0x%x]\n", __FUNCTION__, block_idx, all_blocks[block_idx] << 9);
    }

    file->fd_inode->i_size += count;
    memset(io_buf, 0, BLOCK_SIZE << 1); // inode可能会跨扇区
    inode_sync(cur_part, file->fd_inode, io_buf);

    kmfree_s(io_buf, BLOCK_SIZE << 1);
    kmfree_s(all_blocks, all_block_bytes);

    printk("[%s] ok size: [0x%x -> 0x%x]\n", __FUNCTION__, file->fd_inode->i_size - count, file->fd_inode->i_size);

    return (int32) count;
}


/**
 * 读文件
 * @param file 读哪个文件（包括从哪个位置开始读）
 * @param buf 读到哪里
 * @param count 读多少
 * @return 返回读出的字节数，若到文件尾则返回-1
 */
int32 file_read(file_t *file, void *buf, uint32 count) {
    // 保证能读出数据
    if (file->fd_pos >= file->fd_inode->i_size) {
        printk("[%s] pos err [0x%x / 0x%x]! \n", __FUNCTION__, file->fd_pos, file->fd_inode->i_size);
        return -1;
    }

    // 1，先要计算这次读入需要读几个块，从哪个块的哪个地方开始读，读到哪个块的哪个地方结束

    uint32 start_byte_offset = file->fd_pos;                             // 从文件的哪个字节开始读
    uint8 start_block_offset = start_byte_offset >> 9;                   // 从第几个块开始读
    uint32 start_byte_offset_in_block = start_byte_offset % BLOCK_SIZE;  // 从开始块的第几个字节开始读

    if (start_byte_offset + count > file->fd_inode->i_size) count = file->fd_inode->i_size - start_byte_offset;
    bool read_eof = start_byte_offset + count == file->fd_inode->i_size; // 是否会读到文件的最后一个字节

    uint32 end_byte_offset = start_byte_offset + count - 1;              // 读到文件的哪个字节处截止（包含该字节）
    uint8 end_block_offset = end_byte_offset >> 9;                       // 读到第几个块结束
    uint32 end_byte_offset_in_block = end_byte_offset % BLOCK_SIZE;      // 读到结束块的第几个字节处截止（包含该字节）

    // 2，将需要用的块指针对应的block准备好

    // 用来记录文件所有的块地址（最多支持12+128=140个块）
    uint32 all_block_bytes = end_block_offset < 12 ? 48 : BLOCK_SIZE + 48;
    uint32 *all_blocks = (uint32 *) kmalloc(all_block_bytes);
    memset(all_blocks, 0, all_block_bytes);
    // 直接块中只把用到的同步就行
    for (uint8 block_idx = start_block_offset; block_idx < 12 && block_idx <= end_block_offset; ++block_idx) {
        all_blocks[block_idx] = file->fd_inode->direct_blocks[block_idx];
    }
    // 如果用到间接块，需要先保证间接块已创建，并保证同步
    if (end_block_offset >= 12) {
        // 如果计算需要使用间接块，那它一定有值，否则就是文件大小的记录出错了，需要检查
        if (!file->fd_inode->indirect_block) {
            printk("[%s] record err [0x%x / 0x%x]! \n", __FUNCTION__, end_block_offset << 9, file->fd_inode->i_size);
            STOP
            return -1;
        }
        // 将数据读出
        ide_read(cur_part->disk, file->fd_inode->indirect_block, all_blocks + 12, 1);
    }

    // 3，按块读数据即可

    uint8 *dst = buf;                             // 临时存储每次循环要写入数据的地址
    uint8 *io_buf = kmalloc(BLOCK_SIZE);           // 临时缓冲区
    uint32 item_write_start, item_write_end, item_write_cnt; // 临时变量
    for (uint8 block_idx = start_block_offset; block_idx <= end_block_offset; ++block_idx) {
        // 按块读出数据
        memset(io_buf, 0, BLOCK_SIZE);
        ide_read(cur_part->disk, all_blocks[block_idx], io_buf, 1);

        // 将读出的数据写到缓冲区
        item_write_start = block_idx == start_block_offset ? start_byte_offset_in_block : 0;
        item_write_end = block_idx == end_block_offset ? end_byte_offset_in_block + 1 : BLOCK_SIZE; // 这里不包含结尾字符
        item_write_cnt = item_write_end - item_write_start; // 本次写入字节数

        memcpy(dst, io_buf + item_write_start, item_write_cnt);
        dst += item_write_cnt;

        printk("[%s] from 0x%x: [0x%x]\n", __FUNCTION__, block_idx, all_blocks[block_idx] << 9);
    }

    kmfree_s(io_buf, BLOCK_SIZE);
    kmfree_s(all_blocks, all_block_bytes);

    printk("[%s] ok data: [0x%x]\n", __FUNCTION__, count);
    return read_eof ? -1 : (int32) count;
}

/**
 * 关闭文件
 * @param file 关闭的文件
 * @return 成功返回0，否则返回-1
 */
int32 file_close(file_t *file) {
    if (file == NULL) return -1;
    file->fd_inode->write_deny = false;
    inode_close(cur_part, file->fd_inode);
    file->fd_inode = NULL;
    return 0;
}

