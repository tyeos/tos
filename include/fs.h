//
// Created by toney on 10/2/23.
//

#ifndef TOS_FS_H
#define TOS_FS_H

#include "types.h"
#include "chain.h"


#define MAX_FILE_NAME_LEN 16      // 最大文件名长度
#define MAX_FILES_PER_PART 4096   // 每个分区所支持最大创建的文件数
#define BITS_PER_SECTOR 4096      // 每扇区的位数: 512*8
#define BLOCK_SIZE 512            // 块字节大小 (目前为了简单，设计一个块一个扇区，正常是可以多个扇区的)

#define MAX_PATH_LEN 512 // 路径最大长度

#define FS_TYPE_MAGIC 0x19590318  // 文件系统类型标识

#define MAX_FILE_OPEN 32    //系统可打开的最大文件数

#define STD_FREE_SLOT (-1) // 表示文件描述符可分配，空闲槽位


/*
 * 整体概念：
 * --------------------------------------------------------------------------------------------------------------------
 * 1, 分区、操作系统引导块、超级快、空闲块位图、inode位图、inode数组、根目录、空闲块区域 之间的关系
 *      ---------------------------------------------------
 *      |           | BootSector  (One Block/Sector)      | ( <- low address )
 *      |           |--------------------------------------
 *      |           | SuperBlock  (One Block/Sector)      | -> 记录整个分区的整体规划信息
 *      |           |--------------------------------------
 *      |           | BlockBitmap (Multiple Block/Sector) | -> 将数据区(DataSectors)块个数划分，记录对应块位置的分配情况
 *      |           |--------------------------------------
 *      | Partition | InodeBitmap (Multiple Block/Sector) | -> 将数据区(DataSectors)块inode个数划分，记录对应inode位置的分配情况
 *      |           |--------------------------------------
 *      |           | InodeTable  (Multiple Block/Sector) | -> 以inode数据结构长度为单位划分, 记录文件源信息(数量为分区文件数上限值)
 *      |           |----------------------------------------------------------------
 *      |           |                                 | RootDir (Multiple DirEntry) |
 *      |           | DataSectors (Many Block/Sector) |------------------------------
 *      |           |                                 |       Free Unused ...       | ( <- high address )
 *      -----------------------------------------------------------------------------
 *
 * --------------------------------------------------------------------------------------------------------------------
 * 2，普通文件 与 i节点、数据块 之间的关系：
 *
 *                        ------------------------------                    ----------------------
 *                        |       | ...                |           +------> | Block (BinaryData) |
 *                        |       |---------------------           |        ----------------------
 *  -----------------     |       | *DirectBlocks(x12) | ----------+
 *  |      | *inode | --> | Inode |---------------------           |
 *  | File |---------     |       | *IndirectBlock     | -----+    +-------> ... (Block x11)
 *  |      |  ...   |     |       |---------------------      |
 *  -----------------     |       | ...                |      |
 *                        ------------------------------      ↓
 *                                                        ----------------------      ----------------------
 *                                                        |       | *Block 0   | ---> | Block (BinaryData) |
 *                                                        |       |-------------      ----------------------
 *                                                        | Block | *Block 1   | ... (Block)
 *                                                        |       |-------------
 *                                                        |       |    ...     | ... (Block x126)
 *                                                        ----------------------
 *
 * --------------------------------------------------------------------------------------------------------------------
 * 3，目录文件 与 i节点、目录项 之间的关系：
 *                                                                          -----------------------
 *                                                                          |        | DirEntry 1 |
 *                                                                          |        |-------------
 *                                                                +-------> | Block  | DirEntry 2 |
 *                       ------------------------------           |         |        |-------------
 *                       |       | ...                |           |         |        |     ...    |
 *                       |       |---------------------           |         -----------------------
 *  ----------------     |       | *DirectBlocks(x12) | ----------+-------> ... (Block x11)
 *  |     | *inode | --> | Inode |---------------------
 *  | Dir |---------     |       | *IndirectBlock     | -----+                                -----------------------
 *  |     |  ...   |     |       |---------------------      |                                |        | DirEntry 1 |
 *  ----------------     |       | ...                |      |                                |        |-------------
 *                       ------------------------------      ↓                        +-----> | Block  | DirEntry 2 |
 *                                                        ----------------------      |       |        |-------------
 *                                                        |       | *Block 0   | -----+       |        |     ...    |
 *                                                        |       |-------------              -----------------------
 *                                                        | Block | *Block 1   | ... (Block)
 *                                                        |       |-------------
 *                                                        |       |    ...     | ... (Block x126)
 *                                                        ----------------------
 *
 * --------------------------------------------------------------------------------------------------------------------
 * 4，目录项 与 目录文件、普通文件 之间的关系：
 *                                                      ----------------
 *                               type = DIRECTORY       |     | *inode | ...
 *                           +------------------------> | Dir |---------
 *                           |                          |     |  ...   |
 * ---------------------     |                          ----------------
 * |          | *inode | ----+
 * |          |---------     |                          -----------------
 * | DirEntry | type   |     |                          |      | *inode | ...
 * |          |---------     +------------------------> | File |---------
 * |          |  ...   |          type = REGULAR        |      |  ...   |
 * ---------------------                                -----------------
 *
 * --------------------------------------------------------------------------------------------------------------------
 */

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
    uint32 direct_blocks[12];   // 12个直接块指针
    uint32 indirect_block;      // 一级间接块指针, 在32位下指针占4字节，所以一个间接块可指向512/4=128个块（二三级同理，暂不实现）

    uint32 i_open_cnts;         // 此文件被打开的次数
    bool write_deny;            // 写文件不能并行，进程写文件前检查此标识
    chain_elem_t inode_tag;     // 用于加入已打开的inode列表
}__attribute__((packed)) inode_t;

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


/* 文件类型 */
enum file_types {
    FT_UNKNOWN,     // 0, 不支持的文件类型
    FT_REGULAR,     // 1, 普通文件
    FT_DIRECTORY    // 2, 目录文件
};

/* 目录项结构 */
typedef struct dir_entry_t {
    uint32 i_no;                  // 文件(普通文件或目录文件)对应的inode编号
    enum file_types f_type;       // 文件类型
    char name[MAX_FILE_NAME_LEN]; // 文件名称
}__attribute__((packed)) dir_entry_t;


/* 标准输入输出描述符 */
enum std_fd {
    stdin_no,   // 0 标准输入
    stdout_no,  // 1 标准输出
    stderr_no   // 2 标准错误
};

/* 用来存储inode位置 */
typedef struct inode_position_t {
    uint32 sec_lba;         // inode所在的扇区号
    uint32 off_size;        // inode在扇区内的字节偏移量
    bool two_sec;           // inode是否跨扇区
} __attribute__((packed)) inode_position_t;

/* 目录结构 */
typedef struct dir_t {
    inode_t *inode;
    uint32 dir_pos;     // 记录目录项在目录内的偏移, 以0为起始, 该值一定为 目录项结构体占用字节大小 的倍数
    uint8 dir_buf[512]; // 目录的数据缓存
}__attribute__((packed)) dir_t;

/* 文件结构 */
typedef struct file_t {
    inode_t *fd_inode;
    uint32 fd_pos;     // 记录当前文件操作的偏移地址，以0为起始，最大为文件大小-1
    uint32 fd_flag;
}__attribute__((packed)) file_t;


/* 位图类型 */
enum bitmap_type {
    INODE_BITMAP,   // inode位图
    BLOCK_BITMAP    // 块位图
};

// 打开文件的选项
enum oflags {
    O_RDONLY,       // 只读, 0b000
    O_WRONLY,       // 只写, 0b001
    O_RDWR,         // 读写, 0b010
    O_CREAT = 4     // 创建, 0b100
};

// 用来记录查找文件过程中已找到的上级路径，也就是查找文件过程中“走过的地方”
typedef struct path_search_record_t {
    char searched_path[MAX_PATH_LEN];   // 查找过程中的父路径
    dir_t *parent_dir;                  // 文件或目录所在的直接父目录
    enum file_types file_type;          // 找到的是普通文件，还是目录，找不到将为未知类型（FT_UNKNOWN)
} __attribute__((packed)) path_search_record_t;

// 文件读写位置偏移量
enum whence { // 从1开始递增
    SEEK_SET = 1,   // offset的参照物是文件开始处，也就是将读写位置指针设置为距文件开头偏移offset个字节处
    SEEK_CUR,       // offset的参照物是当前读写位置，也就是将读写位置指针设置为当前位置+offset
    SEEK_END        // offset的参照物是文件尺寸大小，即文件最后一个字节的下一个字节，也就是将读写位置指针设置为文件大小+offset
};

// 文件属性结构体
typedef struct stat_t {
    uint32 st_ino;      //inode编号
    uint32 st_size;     //尺寸
    enum file_types st_filetype;//文件类型
} __attribute__((packed)) stat_t;

void file_sys_init(chain_t *partitions);

int32 sys_open(const char *pathname, uint8 flags);

int32 sys_write(int32 fd, const void *buf, uint32 count);

int32 sys_read(int32 fd, void *buf, uint32 count);

int32 sys_seek(int32 fd, int32 offset, uint8 whence);

int32 sys_close(int32 fd);

int32 sys_unlink(const char *pathname);

int32 sys_mkdir(const char *pathname);

dir_t *sys_opendir(const char *name);

dir_entry_t *sys_readdir(dir_t *dir);

void sys_rewinddir(dir_t *dir);

int32 sys_closedir(dir_t *dir);

int32 sys_rmdir(const char *pathname);

char *sys_getcwd(char *buf, uint32 size);

int32 sys_chdir(const char *path);

int32 sys_stat(const char *path, stat_t *buf);

#endif //TOS_FS_H
