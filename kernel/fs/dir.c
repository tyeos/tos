//
// Created by Toney on 2023/10/3.
//

#include "../../include/ide.h"
#include "../../include/print.h"
#include "../../include/string.h"
#include "../../include/inode.h"
#include "../../include/dir.h"
#include "../../include/file.h"


extern partition_t *cur_part;

dir_t root_dir; //根目录

// 打开根目录
void open_root_dir(partition_t *part) {
    root_dir.inode = inode_open(part, part->sb->root_inode_no);
    root_dir.dir_pos = 0;
}


/**
 * 打开指定目录
 * @param part 目录所属分区
 * @param inode_no 目录的i节点编号
 * @return 目录指针
 */
dir_t *dir_open(partition_t *part, uint32 inode_no) {
    dir_t *pdir = (dir_t *) kmalloc(sizeof(dir_t));
    pdir->inode = inode_open(part, inode_no);
    pdir->dir_pos = 0;
    return pdir;
}

/**
 * 在指定目录中查找指定名称的目录项(文件或目录)
 * @param part 目录所属的分区
 * @param pdir 在哪个目录中查找
 * @param name 要查找的文件或目录的名称
 * @param dir_e 查找结果对应的目录项信息
 * @return 是否查找到了
 */
bool search_dir_entry(partition_t *part, dir_t *pdir, const char *name, dir_entry_t *dir_e) {
    // 12个直接块 + 128个一级间接块 = 140块, 共560字节
    uint32 block_cnt = 140;         // 该目录下最多可查找多少个块
    uint32 block_bytes = 48 + 512;  // 这些块占用的内存大小
    uint32 *all_blocks = (uint32 *) kmalloc(block_bytes);
    memset(all_blocks, 0, block_bytes);

    // 12个直接块地址
    for (uint32 block_idx = 0; block_idx < 12; block_idx++) {
        all_blocks[block_idx] = pdir->inode->direct_blocks[block_idx];
    }
    // 如果有一级间接块表，则将其所在扇区读入
    if (pdir->inode->indirect_block) ide_read(part->disk, pdir->inode->indirect_block, all_blocks + 12, 1);

    /* 至此，all_blocks存储的就是该文件或目录的所有扇区地址 */

    uint32 dir_entry_size = part->sb->dir_entry_size;    // 1个目录项占用的字节数
    uint32 dir_entry_cnt = SECTOR_SIZE / dir_entry_size; // 1个扇区中存放的目录项的个数（写目录项的时候保证目录项不跨扇区）

    // 1个块对应1个扇区
    uint8 *buf = (uint8 *) kmalloc(SECTOR_SIZE);
    dir_entry_t *p_de = (dir_entry_t *) buf;  // 临时存储读到的目录项

    // 开始在所有块中查找目录项
    for (uint32 block_idx = 0; block_idx < block_cnt; ++block_idx) {
        // 先看该块是否有数据，没有就下一个
        if (all_blocks[block_idx] == 0) {
            block_idx++;
            continue;
        }
        // 读出该块对应的扇区数据
        memset(buf, 0, SECTOR_SIZE);
        ide_read(part->disk, all_blocks[block_idx], buf, 1);
        for (uint32 dir_entry_idx = 0; dir_entry_idx < dir_entry_cnt; dir_entry_idx++) {
            // 若找到，则复制目录项信息，并返回成功
            if (!strcmp(p_de->name, name)) {
                memcpy(dir_e, p_de, dir_entry_size);
                // 释放内存并返回结果
                kmfree_s(buf, SECTOR_SIZE);
                kmfree_s(all_blocks, block_bytes);
                return true;
            }
            // 继续找下一个目录项
            p_de++;
        }
        // 继续找下一个块，目录项指针初始化
        p_de = (dir_entry_t *) buf;
    }
    // 全找完了，没找到，释放内存并返回结果
    kmfree_s(buf, SECTOR_SIZE);
    kmfree_s(all_blocks, block_bytes);
    return false;
}

/**
 * 关闭目录
 * @param dir 关闭哪个目录
 */
void dir_close(partition_t *part, dir_t *dir) {
    // 根目录不能关闭
    if (dir == &root_dir) return;
    inode_close(part, dir->inode);
    kmfree_s(dir, sizeof(dir_t));
}

/**
 * 在内存中初始化目录项p_de
 * @param filename 文件名称
 * @param inode_no 文件inode编号
 * @param file_type 文件类型
 * @param p_de 目录项（初始化的信息就写进这里）
 */
void create_dir_entry(char *filename, uint32 inode_no, uint8 file_type, dir_entry_t *p_de) {
    // 初始化目录项
    p_de->i_no = inode_no;
    p_de->f_type = file_type;
    memcpy(p_de->name, filename, strlen(filename));
}

/**
 * 将目录项写入父目录中（保存到硬盘）
 * @param parent_dir 父目录
 * @param p_de 要保存的目录项
 * @param io_buf 临时缓冲区
 * @return 是否成功
 */
bool sync_dir_entry(dir_t *parent_dir, dir_entry_t *p_de, void *io_buf) {
    inode_t *dir_inode = parent_dir->inode;                 // 目录项保存到那个inode
    uint32 dir_entry_size = cur_part->sb->dir_entry_size;   // 一个目录项 占多少字节
    uint32 dir_entry_cnt = SECTOR_SIZE / dir_entry_size;    // 每个扇区 可以保存多少个 目录项

    //将该目录的所有扇区地址 (12个直接块+128个间接块）存入a11_blocks
    uint32 all_blocks[140] = {0}; // all_blocks保存目录所有的块

    // 将12个直接块存入all_blocks
    for (uint8 block_idx = 0; block_idx < 12; block_idx++) all_blocks[block_idx] = dir_inode->direct_blocks[block_idx];
    // 间接块放到第12个位置，读到再更新
    all_blocks[12] = dir_inode->indirect_block;

    // 12个直接块+128个间接块
    for (uint8 block_idx = 0; block_idx < 140; block_idx++) {
        // 先找新建inode的逻辑
        if (!all_blocks[block_idx]) {
            if (block_idx < 12) {
                // 直接块逻辑
                // 申请块地址, 同步块位图, 并同步到父目录inode内存
                uint32 block_lba = block_bitmap_alloc(cur_part);
                bitmap_sync(cur_part, block_lba - cur_part->sb->data_start_lba, BLOCK_BITMAP);
                dir_inode->direct_blocks[block_idx] = all_blocks[block_idx] = block_lba;
            } else {
                // 间接块逻辑
                if (block_idx == 12) {
                    // 间接块未创建, 需要先创建间接块, 申请块地址, 同步块位图, 并同步到父目录inode内存
                    uint32 block_lba = block_bitmap_alloc(cur_part);
                    bitmap_sync(cur_part, block_lba - cur_part->sb->data_start_lba, BLOCK_BITMAP);
                    dir_inode->indirect_block = block_lba;
                }

                // 申请块地址, 同步块位图, 并同步到间接块（硬盘）
                uint32 block_lba = block_bitmap_alloc(cur_part);
                bitmap_sync(cur_part, block_lba - cur_part->sb->data_start_lba, BLOCK_BITMAP);
                all_blocks[block_idx] = block_lba;
                ide_write(cur_part->disk, dir_inode->indirect_block, all_blocks + 12, 1);
            }

            // 将这一个目录项写到硬盘
            memset(io_buf, 0, SECTOR_SIZE);
            memcpy(io_buf, p_de, dir_entry_size);
            ide_write(cur_part->disk, all_blocks[block_idx], io_buf, 1);

            // 更新目录大小并返回结果
            dir_inode->i_size += dir_entry_size;
            printk("[%s] [%s] save to [0x%x] [0x%x]!\n", __FUNCTION__, p_de->name, all_blocks[block_idx] << 9,
                   block_idx);
            return true;
        }

        // 如果是间接块, 需要将其中128项读出
        if (block_idx == 12) {
            ide_read(cur_part->disk, dir_inode->indirect_block, all_blocks + 12, 1);
            // 这时候第12项地址已经更新了，且一定有值，因为间接块的第0个项必须存在，间接块才会创建
        }

        // 第block_idx块已存在，将其读入内存，然后在该块中循环查找空目录项
        ide_read(cur_part->disk, dir_inode->direct_blocks[block_idx], io_buf, 1);
        dir_entry_t *dir_e = (dir_entry_t *) io_buf;
        for (int dir_entry_idx = 0; dir_entry_idx < dir_entry_cnt; ++dir_entry_idx) {
            // FT_UNKNOWN为0, 无论是初始化，或是删除文件后，都会将f_type置为FT_UNKNOWN
            if ((dir_e + dir_entry_idx)->f_type != FT_UNKNOWN) continue;

            // 找到了可用目录项, 将其保存到磁盘
            memcpy(dir_e + dir_entry_idx, p_de, dir_entry_size);
            ide_write(cur_part->disk, dir_inode->direct_blocks[block_idx], io_buf, 1);

            // 更新目录大小并返回结果
            dir_inode->i_size += dir_entry_size;
            printk("[%s] [%s] save to [0x%x: 0x%x] [0x%x: 0x%x]!\n", __FUNCTION__, p_de->name,
                   all_blocks[block_idx] << 9, dir_entry_idx * dir_entry_size, block_idx, dir_entry_idx);
            return true;
        }

        // 这里说明这个块的目录项已经存满了，继续找下一个
    }

    // 全找完了也没找到可用项
    printk("[%s] directory is full!\n", __FUNCTION__);

    return false;
}