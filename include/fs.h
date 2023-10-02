//
// Created by toney on 10/2/23.
//

#ifndef TOS_FS_H
#define TOS_FS_H

#include "types.h"
#include "chain.h"

#define MAX_FILE_NAME_LEN 16      //最大文件名长度
#define MAX_FILES_PER_PART 4096   // 每个分区所支持最大创建的文件数
#define BITS_PER_SECTOR 4096      // 每扇区的位数: 512*8
#define BLOCK_SIZE SECTOR_SIZE    // 块字节大小 (目前为了简单，设计一个块一个扇区，正常是可以多个扇区的)

#define FS_TYPE_MAGIC 0x19590318  // 文件系统类型标识

/*
 * i节点与数据块间的关系
 * -----------------------------------------------------------
 * 1个inode节点 有 N个数据块指针(指向目录块或文件块):
 *     目录文件数据块:
 *         N个目录项: 用来描述普通文件或另一个目录的inode
 *     普通文件数据块
 * -----------------------------------------------------------
 */


/* 超级块 */
typedef struct super_block_t {
    uint32 magic;               // 文件系统类型标识

    uint32 sec_cnt;             // 本分区总的扇区数
    uint32 inode_cnt;           // 本分区中inode数量
    uint32 part_lba_base;       // 本分区的起始lba地址

    uint32 block_bitmap_lba;    // 块位图本身起始扇区地址
    uint32 block_bitmap_sects;  // 块位图本身占用的扇区数量

    uint32 inode_bitmap_lba;    // i结点位图起始扇区lba地址
    uint32 inode_bitmap_sects;  // i结点位图占用的扇区数量

    uint32 inode_table_lba;     // i结点表起始扇区lba地址
    uint32 inode_table_sects;   // i结点表占用的扇区数量

    uint32 data_start_lba;      // 数据区开始的第一个扇区号
    uint32 data_sects;          // 数据区占用的扇区数量（即块位图能管理的扇区数量）
    uint32 root_inode_no;       // 根目录所在的i结点号
    uint32 dir_entry_size;      // 目录项大小

    uint8 pad[460];             // 凑够512字节1扇区大小
}__attribute__((packed)) super_block_t;


/*
 * inode也是FCB(File Control Block)的一种, 即用于管理、控制文件相关信息的数据结构
 * ----------------------------------------------------------------------------------------------------
 * inode逻辑结构：
 *   --------------------
 *   | i结点编号         |
 *   | 权限             |
 *   | 属主             |
 *   | 时间             |
 *   | 文件大小          |
 *   |  ...             |
 *   | 12个直接块指针     | --> 普通数据块
 *   | 一级间接块索引表指针 | --> (一级)数据块索引表 [*1]-->[*N] 普通数据块
 *   | 二级间接块索引表指针 | --> (二级)数据块索引表 [*1]-->[*N] (一级)数据块索引表 [*1]-->[*N] 普通数据块
 *   | 三级间接块索引表指针 | --> (三级)数据块索引表 [*1]-->[*N] (二级)数据块索引表 [*1]-->[*N] (一级)数据块索引表 [*1]-->[*N] 普通数据块
 *   --------------------
 * ----------------------------------------------------------------------------------------------------
 */
typedef struct inode_t {
    uint32 i_no;                // inode编号
    uint32 i_size;              // 文件大小 或 目录下所有目录项大小之和
    uint32 i_open_cnts;         // 此文件被打开的次数
    bool write_deny;            // 写文件不能并行，进程写文件前检查此标识
    /*
     * 0-11为直接块指针, 12为一级间接块指针（二三级同理，暂不实现）
     * 块地址用4字节表示，一个扇区512字节，所以这里支持的一级间接块的数量是128个，加上直接块12个，
     * 所以这里一个inode共可支持140个块
     */
    uint32 i_sectors[13];
//    chain_elem_t *inode_tag;     // 用于加入已打开的inode列表
}__attribute__((packed)) inode_t;

/* 文件类型 */
enum file_types {
    FT_UNKNOWN,     // 不支持的文件类型
    FT_REGULAR,     // 普通文件
    FT_DIRECTORY    // 目录
};

/* 目录项结构 */
typedef struct dir_entry_t {
    uint32 i_no;                        // 普通文件或目录 对应的inode编号
    char filename[MAX_FILE_NAME_LEN];   // 普通文件或目录 名称
    enum file_types f_type;             // 文件类型
}__attribute__((packed)) dir_entry_t;

/* 目录结构 */
typedef struct dir_t {
    inode_t *inode;
    uint32 dir_pos;     // 记录在目录内的偏移
    uint8 dir_buf[512]; // 目录的数据缓存
}__attribute__((packed)) dir_t;


void file_sys_init(chain_t *partitions);

#endif //TOS_FS_H
