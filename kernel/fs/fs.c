//
// Created by toney on 10/2/23.
//

#include "../../include/fs.h"
#include "../../include/ide.h"
#include "../../include/print.h"
#include "../../include/string.h"
#include "../../include/sys.h"
#include "../../include/dir.h"
#include "../../include/file.h"
#include "../../include/inode.h"

extern dir_t root_dir;
extern file_t file_table[MAX_FILE_OPEN];

partition_t *cur_part; // 当前挂载的分区

/*
 * 挂载指定分区
 */
static void mount_partition(partition_t *part) {
    disk_t *disk = part->disk;

    // 从硬盘上读入的超级块
    super_block_t *sb_buf = (super_block_t *) kmalloc(SECTOR_SIZE);
    memset(sb_buf, 0, SECTOR_SIZE);
    ide_read(disk, part->start_lba + 1, sb_buf, 1);

    // 复制到内存分区的超级块
    part->sb = (super_block_t *) kmalloc(sizeof(super_block_t));
    memcpy(part->sb, sb_buf, sizeof(super_block_t));

    // 将硬盘上的块位图读入到内存分区（块位图比较大，这里就按页申请了）
    part->block_bitmap.map = alloc_kernel_pages((sb_buf->block_bitmap_sects + 7) >> 3);
    ide_read(disk, sb_buf->block_bitmap_lba, part->block_bitmap.map, sb_buf->block_bitmap_sects);
    // 初始化位图
    part->block_bitmap.total = sb_buf->data_sects;
    bitmap_slow_init(&part->block_bitmap);

    // 将硬盘上的inode位图读入到内存分区（inode位图目前设计是1占个扇区）
    part->inode_bitmap.map = kmalloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);
    ide_read(disk, sb_buf->inode_bitmap_lba, part->inode_bitmap.map, sb_buf->inode_bitmap_sects);
    // 初始化位图
    part->inode_bitmap.total = sb_buf->inode_bitmap_sects * SECTOR_SIZE;
    bitmap_slow_init(&part->inode_bitmap);

    // 初始化分区队列
    chain_init(&part->open_inodes);

    // 更新当前挂载分区
    cur_part = part;
    printk("mount %s done!\n", part->name);
}


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
    // 引导块 和 超级块 各占一个扇区
    uint32 boot_sector_sects = 1;
    uint32 super_block_sects = 1;

    // 先计算 inode位图 和 inode数组 占用的扇区数
    uint32 inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);
    uint32 inode_table_sects = DIV_ROUND_UP(sizeof(inode_t) * MAX_FILES_PER_PART, SECTOR_SIZE);

    /*
     * 再计算 空闲块位图 和 空闲块 占用的扇区数:
     *      目前只剩下这两个的扇区数没有确定，假设总扇区数 去掉上面四个已经确定的块占用的扇区数后 剩余的扇区数为 rest_sects,
     *      那么 空闲块位图占用的扇区数(block_bitmap_sects) 和 空闲块占用的扇区数(free_sects) 有以下关系：
     *          block_bitmap_sects * 4096 - (0~4096) = free_sects;
     *          block_bitmap_sects + free_sects = rest_sects;
     *      相当于：
     *          空闲块位图 只要多占用一个扇区就可以多管理 512*8=4096个 空闲块。
     *          这里约定，扇区优先分配给 空闲块位图，因为它负责管理分配，没有它那 空闲块 就无法分配。
     *          所以，一个扇区的空闲块位图 可管理0~4096个 空闲块，
     *          即完全分配后若只剩下一个扇区时，那么分配给 空闲块位图, 这时候已经没有可管理的空闲块，这个扇区只能被空置。
     *      例如：
     *          若 rest_sects=1,        那么 block_bitmap_sects=1, free_sects=0,
     *          若 rest_sects=2~4097,   那么 block_bitmap_sects=1, free_sects=1~4096,
     *          若 rest_sects=4098,     那么 block_bitmap_sects=2, free_sects=4096,
     *          若 rest_sects=4099~8194,那么 block_bitmap_sects=2, free_sects=4097~8192,
     *          若 rest_sects=8195,     那么 block_bitmap_sects=3, free_sects=8192,
     *          ... 以此类推 ...
     *      所以：
     *          block_bitmap_sects = (rest_sects + 4096) / 4097
     *          free_sects = rest_sects - block_bitmap_sects
     */
    uint32 rest_sects = part->sec_cnt - boot_sector_sects - super_block_sects - inode_bitmap_sects - inode_table_sects;
    uint32 block_bitmap_sects = (rest_sects + BITS_PER_SECTOR) / (BITS_PER_SECTOR + 1);
    uint32 free_sects = rest_sects - block_bitmap_sects;

    // 超级块初始化
    super_block_t sb;
    sb.magic = FS_TYPE_MAGIC;
    sb.sec_cnt = part->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;
    sb.part_lba_base = part->start_lba;

    sb.block_bitmap_lba = sb.part_lba_base + boot_sector_sects + super_block_sects;
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
    uint32 super_block_lba = part->start_lba + 1;
    ide_write(hd, super_block_lba, &sb, super_block_sects);
    printk("[%s] %s write super block: 0x%x\n", __FUNCTION__, part->name, super_block_lba << 9);

    // ---------- 2扇区开始为空闲块位图（占多个扇区）----------

    /*
     * 1个Page相当于8个扇区，所以1个Page的位图可以管理8*4096个块，即16MB硬盘空间，所以 空闲块位图 一般都是占多页，
     * inode数组更大，目前文件上限数是4096, 即inode_t结构占用多少个字节，inode数组就需要占用多少页内存，
     * 下面要给 空闲块位图、inode位图、inode数组 初始化，所以需要用临时内存将数据写到磁盘，
     * 这里就按最大的申请一个临时共用做临时存储缓冲区。
     */
    uint32 max_sects = sb.block_bitmap_sects > sb.inode_table_sects ? sb.block_bitmap_sects : sb.inode_table_sects;
    max_sects = max_sects > sb.inode_bitmap_sects ? max_sects : sb.inode_bitmap_sects;
    uint32 alloc_pages = (max_sects + 7) >> 3; // 1个page = 8个扇区
    uint8 *buf = alloc_kernel_pages(alloc_pages);

    // 初始化块位图
    memset(buf, 0, sb.block_bitmap_sects << 9);
    buf[0] |= 0b1; // 第0个块预留给根目录，位图中先占位
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
    i->i_no = 0;                             // 根目录占inode数组中第0个inode
    i->i_size = sb.dir_entry_size * 2;       // 根目录下初始有两个目录项 .和..
    i->direct_blocks[0] = sb.data_start_lba; // 指向空闲区起始位置，i_sectors数组的其他元素目前都为0
    ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);
    printk("[%s] %s write inode table: 0x%x\n", __FUNCTION__, part->name, sb.inode_table_lba << 9);

    // ---------- 根目录（占1个扇区）----------

    // 写入根目录的两个目录项.和..
    memset(buf, 0, SECTOR_SIZE); // 等会要以扇区为单位写盘
    dir_entry_t *p_de = (dir_entry_t *) buf;

    // 初始化当前目录"."
    p_de->i_no = 0;
    memcpy(p_de->name, ".", 1);
    p_de->f_type = FT_DIRECTORY;

    // 初始化当前目录父目录".."
    p_de++;
    p_de->i_no = 0; // 根目录的父目录依然是根目录自己
    memcpy(p_de->name, "..", 2);
    p_de->f_type = FT_DIRECTORY;

    // 根目录分配在空闲数据区的起始位置，这里保存根目录的目录项
    ide_write(hd, sb.data_start_lba, buf, 1);
    printk("[%s] %s write root dir: 0x%x\n", __FUNCTION__, part->name, sb.data_start_lba << 9);

    free_kernel_pages(buf, alloc_pages);
}

/**
 * 检查分区是否安装了文件系统，若未安装，则进行格式化分区，创建文件系统
 * @param partitions 要检查的分区列表
 */
void file_sys_init(chain_t *partitions) {
    printk("[%s] searching %d partitions...\n", __FUNCTION__, partitions->size);
    if (!partitions->size) {
        printk("[%s] not fount partitions!\n", __FUNCTION__);
        STOP
        return;
    }
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

    // 挂载分区（这里挂第一个分区了）
    mount_partition(chain_read_first(partitions)->value);

    // 将当前分区的根目录打开
    open_root_dir(cur_part);
    // 初始化文件表
    uint32 fd_idx = 0;
    while (fd_idx < MAX_FILE_OPEN) {
        file_table[fd_idx++].fd_inode = NULL;
    }
}


/**
 * 解析最上层路径的名称
 * @param pathname 要解析的路径，如"/a/b/c"
 * @param name_store 存于存储最上层路径的名称，如"/a/b/c"最终会存入"a"
 * @return 未解析的路径，，如"/a/b/c"最终会返回"/b/c"
 */
// 将最上层路径名称解析出来
static char *path_parse(char *pathname, char *name_store) {
    // 先处理最前面的斜杠, 包括连续的, 如"///a/b"会处理成"a/b"
    if (pathname[0] == '/') while (*(++pathname) == '/');
    // 开始一般的路径解析
    while (*pathname != '/' && *pathname != EOS) *name_store++ = *pathname++;
    return pathname[0] ? pathname : NULL; // 若路径字符串为空，则返回NULL
}

/**
 * 计算路径深度
 * @param pathname 要计算的路径
 * @return 路径深度，说白了就是能解析出几个有效文件或目录名称，如/a/b/c/, 深度为3
 */
static uint32 path_depth_cnt(char *pathname) {
    char name[MAX_FILE_NAME_LEN]; // 用于path parse的参数做路径解析
    uint32 depth = 0;
    while (pathname) {
        memset(name, 0, MAX_FILE_NAME_LEN);
        pathname = path_parse(pathname, name);
        if (!name[0]) break;
        depth++;
    }
    return depth;
}

// 搜索文件pathname,若找到则返回其inode号，否则返回-1
/**
 * 搜索文件
 * @param pathname 要搜索的文件路径
 * @param searched_record 用于存储搜索记录
 * @return 若找到则返回其inode编号，否则返回 ERR_IDX
 */
static uint32 search_file(const char *pathname, path_search_record_t *searched_record) {
    // 若是这三种类型的任意一种，说明是根目录，直接返回
    if (!strcmp(pathname, "/") || !strcmp(pathname, "/.") || !strcmp(pathname, "/..")) {
        searched_record->parent_dir = &root_dir;
        searched_record->file_type = FT_DIRECTORY;
        searched_record->searched_path[0] = 0;      // 搜索路径置空
        return 0;
    }

    // 按层级解析目录
    searched_record->parent_dir = &root_dir; // 父目录默认指向根目录，找到一级更新一次
    dir_entry_t dir_e;                       // 临时存储各级目录项
    char name[MAX_FILE_NAME_LEN] = {0};      // 临时记录各级名称

    // 只要当前路径正常解析出名称，就继续
    for (char *sub_path = path_parse((char *) pathname, name); name[0]; sub_path = path_parse(sub_path, name)) {
        // 将已解析的内容存到搜索记录中
        strcat(searched_record->searched_path, "/");
        strcat(searched_record->searched_path, name);

        // 在目录中查找目录项，若找不到，则返回-1（这时候parent_dir暂不关闭，因为下一步一般就是创建）
        if (!search_dir_entry(cur_part, searched_record->parent_dir, name, &dir_e)) return ERR_IDX;

        // 如果找到了普通文件，就直接返回
        if (FT_REGULAR == dir_e.f_type) {
            searched_record->file_type = FT_REGULAR;
            return dir_e.i_no;
        }

        // 否则就是目录（暂不考虑未知类型）
        dir_close(cur_part, searched_record->parent_dir);                                 // 关闭已查找的目录
        searched_record->parent_dir = dir_open(cur_part, dir_e.i_no); // 更新父目录到当前目录

        // 如果没有可解析的下级目录(或普通文件)了，那要的就是这个目录，到此结束
        if (!sub_path) break; // 这里考虑目录最后可能是以斜杠结尾，所以在最后统一返回
        // 还没解析完，就继续找
        memset(name, 0, MAX_FILE_NAME_LEN);
    }

    // 到这里就说明是目录了，没找到文件
    searched_record->file_type = FT_DIRECTORY;
    return dir_e.i_no;
}


/**
 * 打开或创建普通文件
 * @param pathname 普通文件路径
 * @param flags 枚举 oflags 可以是组合值
 * @return 成功返回文件描述符，否则返回-1
 */
int32 sys_open(const char *pathname, uint8 flags) {
    // 这里只支持普通文件的打开，对于目录要用dir_open
    if (pathname[strlen(pathname) - 1] == '/') {
        printk("[%s] can't open a directory: %s\n", __FUNCTION__, pathname);
        STOP
        return -1;
    }

    // 查找文件
    path_search_record_t searched_record;
    memset(&searched_record, 0, sizeof(path_search_record_t));
    uint32 inode_no = search_file(pathname, &searched_record);
    // 不管有没有找到，首先要确保找的结果不是目录
    if (searched_record.file_type == FT_DIRECTORY) {
        printk("[%s] found unsupported directory: %s\n", __FUNCTION__, pathname);
        dir_close(cur_part, searched_record.parent_dir);
        STOP
        return -1;
    }

    // 其次，已搜索的层级要是最后一级
    uint32 pathname_depth = path_depth_cnt((char *) pathname);
    uint32 path_searched_depth = path_depth_cnt(searched_record.searched_path);
    if (pathname_depth != path_searched_depth) {
        // 说明并没有访问到全部的路径，某个中间目录是不存在的
        printk("[%s] parent directory [%s] does not exist!\n", __FUNCTION__, searched_record.searched_path);
        dir_close(cur_part, searched_record.parent_dir);
        return -1;
    }

    bool found = inode_no != (uint32) ERR_IDX ? true : false;

    // 如果文件存在，不可重复创建
    if (found && (flags & O_CREAT)) {
        printk("[%s] %s has already exist ~\n", __FUNCTION__, pathname);
        dir_close(cur_part, searched_record.parent_dir);
        return -1;
    }
    // 如果文件不存在，必须先创建
    if (!found && !(flags & O_CREAT)) {
        printk("[%s] %s doesn't exist ~\n", __FUNCTION__, pathname);
        dir_close(cur_part, searched_record.parent_dir);
        return -1;
    }

    // 其他情况正常给数据
    int32 fd;
    if (flags & O_CREAT) {
        // 创建文件
        printk("[%s] [%s] creating ~\n", __FUNCTION__, pathname);
        fd = file_create(searched_record.parent_dir, strrchr(searched_record.searched_path, '/') + 1, flags);
        dir_close(cur_part, searched_record.parent_dir);
    } else {
        // 其余情况均为打开已存在文件 O_RDONLY, O_WRONLY, O_RDWR
        fd = file_open(inode_no, flags);
    }

    // 此fd是指任务pcb->fd_table数组中的元素下标，并不是指全局file_table中的下标
    return fd;
}

// 将buf中连续count个字节写入文件描述符fd, 成功则返回写入的字节数，失败返回-1
int32 sys_write(int32 fd, const void *buf, uint32 count) {
    if (fd < 0) {
        printk("[%s] fd error: %d\n", __FUNCTION__, fd);
        STOP
        return -1;
    }
    if (fd == stdout_no) {
        char tmp_buf[1024] = {0};
        memcpy(tmp_buf, buf, count);
        printk(tmp_buf);
        return (int32) count;
    }
    uint32 _fd = get_current_task()->fd_table[fd];
    file_t *wr_file = &file_table[_fd];

    if (wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR) {
        int32 bytes_written = file_write(wr_file, buf, count);
        return bytes_written;
    }
    printk("[%s] not allowed to write file without flag: %d\n", __FUNCTION__, wr_file->fd_flag);
    STOP
    return -1;
}

// 从文件描述符fd指向的文件中读取count个字节到buf, 若成功则返回读出的字节数，到文件尾则返回-1
int32 sys_read(int32 fd, void *buf, uint32 count) {
    if (fd < 0) {
        printk("[%s] fd error: %d\n", __FUNCTION__, fd);
        STOP
        return -1;
    }
    uint32 _fd = get_current_task()->fd_table[fd];
    return file_read(&file_table[_fd], buf, count);
}

// 重置用于文件读写操作的偏移指针, 成功时返回新的偏移量，出错时返回-1
int32 sys_seek(int32 fd, int32 offset, uint8 whence) {
    if (fd < 0) {
        printk("[%s] fd error: %d\n", __FUNCTION__, fd);
        STOP
        return -1;
    }
    uint32 _fd = get_current_task()->fd_table[fd];
    file_t *pf = &file_table[_fd];
    int32 new_pos = 0;    // 新的偏移量必须位于文件大小之内
    int32 file_size = (int32) pf->fd_inode->i_size;

    switch (whence) {
        case SEEK_SET: // SEEK_SET新的读写位置是相对于文件开头再增加offset个位移量
            new_pos = offset;
            break;
        case SEEK_CUR:// SEEK_CUR新的读写位置是相对于当前的位置增加offset个位移量
            new_pos = (int32) pf->fd_pos + offset; // offset可正可负
            break;
        case SEEK_END:// SEEK_END新的读写位置是相对于文件尺寸再增加offset个位移量
            new_pos = file_size + offset; // 此情况下，offset应该为负值
            break;
        default:
            printk("[%s] whence error: %d\n", __FUNCTION__, whence);
            STOP
            break;
    }
    if (new_pos < 0 || new_pos > (file_size - 1)) {
        return -1;
    }
    pf->fd_pos = new_pos;
    return (int32) pf->fd_pos;
}

// 关闭文件描述符fd指向的文件，成功返回0, 否则返回-1
int32 sys_close(int32 fd) {
    if (fd < 3) return -1;
    uint32 _fd = get_current_task()->fd_table[fd];
    int32 ret = file_close(&file_table[_fd]);
    get_current_task()->fd_table[fd] = STD_FREE_SLOT; // 使该文件描述符位可用
    return ret;
}


// 删除文件（非目录）,成功返回0,失败返回-1
int32 sys_unlink(const char *pathname) {

    // 1, 确保文件存在
    path_search_record_t searched_record;
    memset(&searched_record, 0, sizeof(path_search_record_t));

    uint32 inode_no = search_file(pathname, &searched_record);
    if (inode_no == (uint32) ERR_IDX) {
        printk("[%s] file not found: %s\n", __FUNCTION__, pathname);
        dir_close(cur_part, searched_record.parent_dir);
        return -1;
    }
    if (searched_record.file_type == FT_DIRECTORY) {
        // 删除目录用 rmdir
        printk("[%s] can't delete a dir: %s\n", __FUNCTION__, pathname);
        dir_close(cur_part, searched_record.parent_dir);
        STOP
        return -1;
    }

    // 2, 检查是否在已打开文件列表（文件表）中, 打开的文件不允许删除

    for (uint8 file_idx = 0; file_idx < MAX_FILE_OPEN; ++file_idx) {
        if (file_table[file_idx].fd_inode != NULL && inode_no == file_table[file_idx].fd_inode->i_no) {
            dir_close(cur_part, searched_record.parent_dir);
            printk("[%s] not allowed to delete while in use: %s\n", __FUNCTION__, pathname);
            return -1;
        }
    }

    // 3, 删除目录项，释放inode
    void *io_buf = kmalloc(SECTOR_SIZE << 1);
    delete_dir_entry(cur_part, searched_record.parent_dir, inode_no, io_buf);
    inode_release(cur_part, inode_no);
    dir_close(cur_part, searched_record.parent_dir);

    kmfree_s(io_buf, SECTOR_SIZE << 1);
    return 0;  // 成功删除
}

// 创建目录pathname, 成功返回0, 失败返回-1
int32 sys_mkdir(const char *pathname) {
    // 1, 确认待创建的新目录在文件系统上不存在
    path_search_record_t searched_record;
    memset(&searched_record, 0, sizeof(path_search_record_t));
    uint32 inode_no = search_file(pathname, &searched_record);
    if (inode_no != (uint32) ERR_IDX) {
        printk("[%s] file or directory exist: %s\n", __FUNCTION__, pathname);
        return -1;
    }

    // 从这里开始，后面的失败需要记录回滚
    uint8 rollback_step = 0;

    // 确保已搜索的层级是最后一级
    uint32 pathname_depth = path_depth_cnt((char *) pathname);
    uint32 path_searched_depth = path_depth_cnt(searched_record.searched_path);
    if (pathname_depth != path_searched_depth) {
        // 说明并没有访问到全部的路径，某个中间目录是不存在的
        printk("[%s] parent directory does not exist: %s\n", __FUNCTION__, searched_record.searched_path);
        rollback_step = 1;
        goto rollback;
    }

    // 2, 为新目录创建inode

    dir_t *parent_dir = searched_record.parent_dir;
    char *dirname = strrchr(searched_record.searched_path, '/') + 1;
    inode_no = inode_bitmap_alloc(cur_part); // 最后统一同步到硬盘
    inode_t new_dir_inode;
    inode_init(inode_no, &new_dir_inode); // 初始化i结点

    // 3, 为新目录分配1个块存储该目录中的目录项

    uint32 block_lba = block_bitmap_alloc(cur_part); // 最后统一同步到硬盘
    uint32 block_bitmap_idx = block_lba - cur_part->sb->data_start_lba; // 用来记录block对应于block_bitmap中的索引
    new_dir_inode.direct_blocks[0] = block_lba;

    // 4, 在新目录中创建两个目录项“.”和“..”，这是每个目录都必须存在的两个目录项

    uint32 io_buf_size = SECTOR_SIZE << 1;
    void *io_buf = kmalloc(io_buf_size);
    memset(io_buf, 0, io_buf_size);
    dir_entry_t *p_de = (dir_entry_t *) io_buf;

    // 初始化当前目录"."
    p_de->i_no = inode_no;
    p_de->f_type = FT_DIRECTORY;
    memcpy(p_de->name, ".", 1);

    // 初始化当前目录".."
    p_de++;
    p_de->i_no = parent_dir->inode->i_no;
    p_de->f_type = FT_DIRECTORY;
    memcpy(p_de->name, "..", 2);

    // 将当前目录的目录项'.'和'..'写入目录
    ide_write(cur_part->disk, new_dir_inode.direct_blocks[0], io_buf, 1);
    new_dir_inode.i_size = cur_part->sb->dir_entry_size << 1;

    // 5, 在新目录的父目录中添加新目录的目录项

    dir_entry_t new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(dir_entry_t));
    create_dir_entry(dirname, inode_no, FT_DIRECTORY, &new_dir_entry);

    // 在父目录中添加自己的目录项
    memset(io_buf, 0, io_buf_size);
    if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) { // sync_dir_entry中将block_bitmap通过bitmap_sync同步到硬盘
        printk("[%s] sync_dir_entry to disk err!\n", __FUNCTION__);
        rollback_step = 2;
        goto rollback;
    }

    // 6, 将以上资源的变更同步到硬盘

    // 父目录的inode同步到硬盘
    memset(io_buf, 0, io_buf_size);
    inode_sync(cur_part, parent_dir->inode, io_buf);

    // 将新创建目录的inode同步到硬盘
    memset(io_buf, 0, io_buf_size);
    inode_sync(cur_part, &new_dir_inode, io_buf);

    // 将 块位图 和 inode位图 同步到硬盘
    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);

    // 关闭所创建目录的父目录
    dir_close(cur_part, searched_record.parent_dir);

    kmfree_s(io_buf, io_buf_size);
    return 0;

    // 7, 若中间有步骤失败，统一处理回滚时间
    rollback:
    switch (rollback_step) {
        case 2:
            inode_bitmap_free(cur_part, inode_no);
            block_bitmap_free(cur_part, block_lba);
            kmfree_s(io_buf, io_buf_size);
        case 1:
            dir_close(cur_part, searched_record.parent_dir);
        default:
            break;
    }
    return -1;

}


// 目录打开成功后返回目录指针，失败返回NULL
dir_t *sys_opendir(const char *name) {
    // 检查是否是根目录
    if (name[0] == '/' && (name[1] == 0 || name[0] == '.')) {
        return &root_dir;
    }

    // 检查待打开的目录是否存在
    path_search_record_t searched_record;
    memset(&searched_record, 0, sizeof(path_search_record_t));
    uint32 inode_no = search_file(name, &searched_record);
    if (inode_no == (uint32) ERR_IDX) {
        printk("[%s] directory not exist: %s\n", __FUNCTION__, searched_record.searched_path);
        return NULL;
    }

    // 确认找到的是目录
    if (searched_record.file_type != FT_DIRECTORY) {
        printk("[%s] not a directory: %s\n", __FUNCTION__, name);
        return NULL;
    }

    dir_t *ret = dir_open(cur_part, inode_no);
    dir_close(cur_part, searched_record.parent_dir);
    return ret;
}

// 读取目录dir的1个目录项，成功后返回其目录项地址，到目录尾时或出错时返回NULL
dir_entry_t *sys_readdir(dir_t *dir) {
    return dir ? dir_read(dir) : NULL;
}

// 把目录dir的指针dir_pos置0
void sys_rewinddir(dir_t *dir) {
    dir->dir_pos = 0;
}

// 成功关闭目录p_dir返回0,失败返回-1
int32 sys_closedir(dir_t *dir) {
    if (dir == NULL) return -1;
    dir_close(cur_part, dir);
    return 0;
}

// 删除空目录，成功时返回0,失败时返回-1
int32 sys_rmdir(const char *pathname) {
    // 先检查待删除的文件是否存在
    path_search_record_t searched_record;
    memset(&searched_record, 0, sizeof(path_search_record_t));
    uint32 inode_no = search_file(pathname, &searched_record);
    uint8 close_step = 0;
    if (inode_no == (uint32) ERR_IDX) {
        printk("[%s] not found dir: %s\n", __FUNCTION__, pathname);
        goto to_close;
    }
    // 确保不是删除根目录
    if (!inode_no) {
        printk("[%s] cannot delete root dir: %s\n", __FUNCTION__, pathname);
        goto to_close;
    }
    // 确保删除的是目录
    if (searched_record.file_type != FT_DIRECTORY) {
        printk("[%s] not a dir %s\n", __FUNCTION__, pathname);
        goto to_close;
    }
    // 确保为空目录
    dir_t *dir = dir_open(cur_part, inode_no);
    if (!dir_is_empty(dir)) {
        printk("[%s] not empty [%s: %d]\n", __FUNCTION__, pathname, dir->inode->i_size / cur_part->sb->dir_entry_size);
        close_step = 1;
        goto to_close;
    }

    // 删除目录
    if (dir_remove(searched_record.parent_dir, dir)) {
        printk("[%s] fail %s\n", __FUNCTION__, pathname);
        close_step = 1;
        goto to_close;
    }

    // 删除成功
    close_step = 2;

    // 回收资源
    to_close:
    switch (close_step) {
        case 2:
        case 1:
            dir_close(cur_part, dir);
        case 0:
            dir_close(cur_part, searched_record.parent_dir);
            break;
        default:
            break;
    }

    return close_step == 2 ? 0 : -1;
}

// 获得父目录的inode编号
static uint32 get_parent_dir_inode_nr(uint32 child_inode_nr, void *io_buf) {
    // 目录中的目录项".."中包括父目录inode编号，"."位于目录的第0块
    inode_t *child_dir_inode = inode_open(cur_part, child_inode_nr);
    uint32 block_lba = child_dir_inode->direct_blocks[0];
    inode_close(cur_part, child_dir_inode);

    // 第0个目录项是".",第1个目录项是".."
    ide_read(cur_part->disk, block_lba, io_buf, 1);
    dir_entry_t *dir_e = (dir_entry_t *) io_buf;

    // 返回..即父目录的inode编号
    return dir_e[1].i_no;
}

/**
 * 在父目录中查找子目录
 * @param p_inode_nr 父目录的inode编号
 * @param c_inode_nr 子目录的inode编号
 * @param path 结果缓冲区，往后追加存入查到的子目录名称
 * @param io_buf 临时缓冲区
 * @return 成功返回0, 失败返-1
 */
static int get_child_dir_name(uint32 p_inode_nr, uint32 c_inode_nr, char *path, void *io_buf) {

    // 1, 先收集父目录的的全部块地址
    inode_t *parent_dir_inode = inode_open(cur_part, p_inode_nr);
    uint32 all_blocks[140] = {0};
    for (uint8 block_idx = 0; block_idx < 12; ++block_idx) {
        all_blocks[block_idx] = parent_dir_inode->direct_blocks[block_idx];
    }
    if (parent_dir_inode->indirect_block) {
        ide_read(cur_part->disk, parent_dir_inode->indirect_block, all_blocks + 12, 1);
    }
    inode_close(cur_part, parent_dir_inode);

    // 2，遍历所有块 遍历每个块中的所有目录项
    uint32 dir_entry_size = cur_part->sb->dir_entry_size;    // 1个目录项占用的字节数
    uint32 dir_entry_cnt = SECTOR_SIZE / dir_entry_size;     // 1个扇区中存放的目录项的最大个数（目录项不跨扇区）

    dir_entry_t *p_de = (dir_entry_t *) io_buf; // 临时记录块中的每一个目录项
    for (uint8 block_idx = 0; block_idx < 140; ++block_idx) {
        // 先看该块是否有数据，没有就下一个
        if (all_blocks[block_idx] == 0) continue;

        memset(io_buf, 0, SECTOR_SIZE);
        ide_read(cur_part->disk, all_blocks[block_idx], io_buf, 1);

        for (uint32 dir_entry_idx = 0; dir_entry_idx < dir_entry_cnt; dir_entry_idx++) {
            // 先确定文件可用
            if (!p_de->f_type) continue;

            // 找到了
            if (p_de[dir_entry_idx].i_no == c_inode_nr) {
                strcat(path, "/");
                strcat(path, p_de[dir_entry_idx].name);
                return 0;
            }
        }
    }
    return -1;
}

// 把当前工作目录绝对路径写入buf, size是buf的大小，失败返回NULL
char *sys_getcwd(char *buf, uint32 size) {
    uint32 child_inode_nr = get_current_task()->cwd_inode_nr;

    // 若当前目录是根目录，直接返回'/'
    if (child_inode_nr == 0) {
        buf[0] = '/';
        buf[1] = 0;
        return buf;
    }

    void *io_buf = kmalloc(SECTOR_SIZE);
    char full_path_reverse[MAX_PATH_LEN] = {0}; // 临时用来做全路径缓冲区
    uint32 parent_inode_nr;
    while (true) {
        // 获取父目录inode编号
        parent_inode_nr = get_parent_dir_inode_nr(child_inode_nr, io_buf);

        // 从父目录中获取当前目录项名称
        if (get_child_dir_name(parent_inode_nr, child_inode_nr, full_path_reverse, io_buf)) {
            // 未获取到, 检查程序逻辑
            kmfree_s(io_buf, SECTOR_SIZE);
            printk("[%s] name err\n", __FUNCTION__);
            STOP
            return NULL;
        }

        // 已经找到根目录了
        if (!parent_inode_nr) break;

        // 往上再找一级，直到inode编号为0即根目录为止
        child_inode_nr = parent_inode_nr;
    }
    kmfree_s(io_buf, SECTOR_SIZE);

    // 因为是从子目录往父目录按层级找的，所以路径也是反着存的，即子目录在前（左）,父目录在后（右）, 所以下面反转路径
    memset(buf, 0, size);
    char *last_slash; // 临时用于记录字符串中最后一个斜杠地址
    while ((last_slash = strrchr(full_path_reverse, '/'))) {
        strcat(buf, last_slash);
        *last_slash = EOS; // 标记字符串结尾
    }
    return buf;
}

// 更改(change)当前工作目录为绝对路径path, 成功则返回0, 失败返回-1
int32 sys_chdir(const char *path) {
    int32 ret = -1;

    // 直接搜索
    path_search_record_t searched_record;
    memset(&searched_record, 0, sizeof(path_search_record_t));
    uint32 inode_no = search_file(path, &searched_record);
    if (inode_no == (uint32) ERR_IDX) {
        printk("[%s] not found: %s\n", __FUNCTION__, path);
    } else {
        if (searched_record.file_type != FT_DIRECTORY) {
            printk("[%s] not a directory: %s\n", __FUNCTION__, path);
        } else {
            // 找到后赋值给当前任务即可
            get_current_task()->cwd_inode_nr = inode_no;
            ret = 0;
        }
    }

    dir_close(cur_part, searched_record.parent_dir);
    return ret;
}