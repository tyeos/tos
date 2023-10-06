//
// Created by toney on 10/6/23.
//

#include "../../include/print.h"
#include "../../include/fs.h"
#include "../../include/string.h"
#include "../../include/sys.h"
#include "../../include/types.h"
#include "../../include/console.h"
#include "../../include/fs.h"
#include "../../include/task.h"
#include "../../include/shell.h"

extern char cwd_cache[CWD_SIZE];

// 用于临时存储各业务场景的绝对路径
static char final_path[MAX_PATH_LEN];

/*
 * ps命令的内建函数
 * 使用方式：
 *      ps
 */
static void buildin_ps(uint32 argc, char **argv) {
    if (argc != 1) {
        printk("[%s] unknown operation!\n", __FUNCTION__);
        return;
    }
    sys_ps();
}

// 将路径old_abs_path中的..和.转换为实际路径后存入new_abs_path
static void wash_path(char *old_abs_path, char *new_abs_path) {
    // 先将要返回的路径初始化为根目录
    new_abs_path[0] = '/';
    new_abs_path[1] = 0;

    // 将路径按层级解析，直到最后一级
    char name[MAX_FILE_NAME_LEN] = {0};
    char *sub_path = old_abs_path;
    while (sub_path) {
        // 解析出第一级目录，放到name中
        sub_path = path_parse(sub_path, name);

        // 如果没解析出值，就可以结束了
        if (!name[0]) break;

        // 如果是返回上一级，需要将最右边的斜杠替换为0
        if (!strcmp("..", name)) {
            char *slash_ptr = strrchr(new_abs_path, '/');
            // 如果已经是退到根目录了，那就停在根目录
            if (slash_ptr == new_abs_path) {
                *(slash_ptr + 1) = 0;
                continue;
            }
            // 其他情况就正常处理
            *slash_ptr = 0;
            continue;
        }

        // 如果是当前目录，可直接忽略
        if (!strcmp(".", name)) continue;

        // 其他情况，正常拼接
        if (new_abs_path[1]) strcat(new_abs_path, "/"); // 如果不是第一级, 先拼个斜杠
        strcat(new_abs_path, name); // 拼目录名
        memset(name, 0, MAX_FILE_NAME_LEN);
    }
}

// 将path处理成不含..和.的绝对路径，存储在new_abs_path
static void make_clear_abs_path(char *path, char *new_abs_path) {
    char abs_path[MAX_PATH_LEN] = {0};  // 用于存放临时处理的绝对路径

    // 若输入的不是绝对路径，就拼接成绝对路径
    if (path[0] != '/') {
        // 获取当前路径，存放到abs_path
        if (sys_getcwd(abs_path, MAX_PATH_LEN)) {
            // 若abs_path不是根目录，就在后面拼个斜杠
            if (!(abs_path[0] == '/' && abs_path[1] == 0)) strcat(abs_path, "/");
        }
    }

    // 将原路径拼到后面
    strcat(abs_path, path);
    wash_path(abs_path, new_abs_path);
}

/*
 * ls命令的内建函数
 * 使用方式：
 *      ls {-h |-l} {dir_path | file_path}
 */
static void buildin_ls(uint32 argc, char **argv) {
    char *pathname = NULL;  // 要查看的路径
    bool long_info = false; // 是否输出长信息
    uint32 arg_path_nr = 0; // 路径参数个数

    // 检查参数，命令参数从1开始
    for (uint32 arg_idx = 1; arg_idx < argc; ++arg_idx) {
        // 先看是否是选项
        if (argv[arg_idx][0] == '-') {
            // long
            if (!strcmp("-l", argv[arg_idx])) {
                long_info = true;
                continue;
            }
            // help
            if (!strcmp("-h", argv[arg_idx])) {
                printk("usage: -l list all information about the file.\n"
                       "-h for help\n"
                       "list all files in the current directory if no option\n");
                return;
            }
            // 只支持 -h -l 两个选项
            printk("[%s]: invalid option %s\n"
                   "Try 'ls -h' for more information.\n", __FUNCTION__, argv[arg_idx]);
            return;
        }
        // 保证只有一个参数
        if (arg_path_nr) {
            printk("[%s]: only support one path!\n", __FUNCTION__);
            return;
        }
        pathname = argv[arg_idx];
        arg_path_nr = 1;
    }

    // 处理为绝对路径
    if (pathname) {
        // 如果输入了路径，需要将其中的.和..处理掉
        make_clear_abs_path(pathname, final_path);
    } else {
        // 如果没有输入操作路径，则以当前路径的绝对路径为参数
        if (!sys_getcwd(final_path, MAX_PATH_LEN)) {
            printk("[%s]: get default path fail!\n", __FUNCTION__);
            return;
        }
    }
    pathname = final_path;

    // 获取当前路径信息
    stat_t file_stat;
    memset(&file_stat, 0, sizeof(stat_t));
    if (sys_stat(pathname, &file_stat) == -1) {
        printk("[%s] no such file or directory: %s\n", __FUNCTION__, pathname);
        return;
    }

    // 如果查看的不是目录，输出文件信息即可
    if (file_stat.st_filetype != FT_DIRECTORY) {
        // 非详细信息
        if (!long_info) {
            printk("%s\n", pathname);
            return;
        }
        // 输出详细信息
        printk("- %d %d %s\n", file_stat.st_ino, file_stat.st_size, pathname);
        return;
    }

    // 为目录中的文件做准备，新建路径，在原有路径基础上，后面拼一个斜杠
    char sub_pathname[MAX_PATH_LEN] = {0};
    uint32 pathname_len = strlen(pathname); // 路径长度
    uint32 last_char_idx = pathname_len - 1;    // 路径最后一个字符的位置
    memcpy(sub_pathname, pathname, pathname_len);
    if (sub_pathname[last_char_idx] != '/') {
        sub_pathname[pathname_len] = '/';
        pathname_len++;
    }

    // 打开目录，并将目录指针指向起始位置
    dir_t *dir = sys_opendir(pathname);
    sys_rewinddir(dir);

    // 准备目录项
    dir_entry_t *dir_e = NULL;
    uint32 sub_file_cnt = file_stat.st_size / sizeof(dir_entry_t);

    // 非详细信息
    if (!long_info) {
        // 读取目录项后，输出目录项名称即可
        for (int i = 0; i < sub_file_cnt; ++i) printk("%s ", sys_readdir(dir)->name);
        printk("\n");
        sys_closedir(dir); // 完成后关闭目录
        return;
    }

    // 输出详细信息
    // 先输出目录占用空间大小 和 文件数量
    printk("total: %dB  count: %d\n", file_stat.st_size, sub_file_cnt);
    printk("TYPE    INODE   SIZE    NAME\n", file_stat.st_size, sub_file_cnt);
    for (int i = 0; i < sub_file_cnt; ++i) {
        dir_e = sys_readdir(dir);
        // 生成子文件的绝对路径
        sub_pathname[pathname_len] = 0;
        strcat(sub_pathname, dir_e->name);
        // 读取文件信息
        memset(&file_stat, 0, sizeof(stat_t));
        if (sys_stat(sub_pathname, &file_stat)) {
            printk("[%s] cannot access: %s\n", __FUNCTION__, sub_pathname);
            return;
        }
        // 输出子文件信息, 目录前缀用d, 文件前缀用-
        printk("%-*c %-*d %-*d %s\n", 7, dir_e->f_type == FT_DIRECTORY ? 'd' : '-',
               7, dir_e->i_no, 7, file_stat.st_size, dir_e->name);
    }
    sys_closedir(dir); // 完成后关闭目录
}

/*
 * pwd命令的内建函数
 * 使用方式：
 *      pwd
 */
void buildin_pwd(uint32 argc, char **argv) {
    if (argc != 1) {
        printk("[%s] unknown operation!\n", __FUNCTION__);
        return;
    }

    if (!sys_getcwd(final_path, MAX_PATH_LEN)) {
        printk("[%s] fail!\n", __FUNCTION__);
        return;
    }

    printk("%s\n", final_path);
}

/*
 * cd命令的内建函数
 * 使用方式：
 *      cd {dir_path}
 */
void buildin_cd(uint32 argc, char **argv) {
    if (argc > 2) {
        printk("[%s] unknown operation!\n", __FUNCTION__);
        return;
    }
    if (argc == 1) {
        // 无参数直接返回到根目录
        final_path[0] = '/';
        final_path[1] = 0;
    } else {
        // 有参数则将其转成绝对路径
        make_clear_abs_path(argv[1], final_path);
    }

    // 更新当前任务的inode编号
    if (sys_chdir(final_path)) {
        printk("[%s] no such dir: %s\n", __FUNCTION__, final_path);
        return;
    }

    // 更新命令行缓存
    memset(cwd_cache, 0, CWD_SIZE);
    strcpy(cwd_cache, final_path);
}

/*
 * mkdir命令的内建函数
 * 使用方式：
 *      mkdir [dir_path]
 */
void buildin_mkdir(uint32 argc, char **argv) {
    if (argc != 2 || !argv[1]) {
        printk("[%s] unknown operation!\n", __FUNCTION__);
        return;
    }

    // 处理为绝对路径
    make_clear_abs_path(argv[1], final_path);

    // 创建目录
    if (sys_mkdir(final_path) == -1) {
        printk("[%s] err !\n", __FUNCTION__);
        return;
    }

    printk("[%s] ok: %s\n", __FUNCTION__, final_path);
}

/*
 * rmdir命令的内建函数
 * 使用方式：
 *      rmdir [dir_path]
 */
void buildin_rmdir(uint32 argc, char **argv) {
    if (argc != 2 || !argv[1]) {
        printk("[%s] unknown operation!\n", __FUNCTION__);
        return;
    }

    // 处理为绝对路径
    make_clear_abs_path(argv[1], final_path);

    // 删除目录
    if (sys_rmdir(final_path) == -1) {
        printk("[%s] err !\n", __FUNCTION__);
        return;
    }

    printk("[%s] ok: %s\n", __FUNCTION__, final_path);
}

/*
 * touch命令的内建函数
 * 使用方式：
 *      touch [file_path]
 */
void buildin_touch(uint32 argc, char **argv) {
    if (argc != 2 || !argv[1]) {
        printk("[%s] unknown operation!\n", __FUNCTION__);
        return;
    }

    // 处理为绝对路径
    make_clear_abs_path(argv[1], final_path);

    // 创建文件
    int32 fd = sys_open(final_path, O_CREAT);
    if (fd == -1) {
        printk("[%s] err !\n", __FUNCTION__);
        return;
    }

    // 关闭文件
    if (sys_close(fd) == -1) {
        printk("[%s] close err !\n", __FUNCTION__);
        return;
    }

    printk("[%s] ok: %s\n", __FUNCTION__, final_path);
}


/*
 * rm命令的内建函数
 * 使用方式：
 *      rm [file_path]
 */
void buildin_rm(uint32 argc, char **argv) {
    if (argc != 2 || !argv[1]) {
        printk("[%s] unknown operation!\n", __FUNCTION__);
        return;
    }

    // 处理为绝对路径
    make_clear_abs_path(argv[1], final_path);

    // 删除文件
    if (sys_unlink(final_path) == -1) {
        printk("[%s] err !\n", __FUNCTION__);
        return;
    }

    printk("[%s] ok: %s\n", __FUNCTION__, final_path);
}

/*
 * write命令的内建函数
 * 使用方式：
 *      write [file_path] [content]
 */
void buildin_write(uint32 argc, char **argv) {
    if (argc != 3 || !argv[1]) {
        printk("[%s] unknown operation!\n", __FUNCTION__);
        return;
    }

    uint32 content_size = strlen(argv[2]);
    if (!content_size) {
        printk("[%s] no input content!\n", __FUNCTION__);
        return;
    }

    // 处理为绝对路径
    make_clear_abs_path(argv[1], final_path);

    // 打开文件
    int32 fd = sys_open(final_path, O_RDWR);
    if (fd == -1) {
        printk("[%s] open err !\n", __FUNCTION__);
        return;
    }

    // 写入内容（追加写）
    int32 cnt = sys_write(fd, argv[2], content_size);
    if (cnt == -1) {
        printk("[%s] err !\n", __FUNCTION__);
        sys_close(fd);
        return;
    }

    // 关闭文件
    if (sys_close(fd) == -1) {
        printk("[%s] close err !\n", __FUNCTION__);
        return;
    }

    printk("[%s] ok: %d -> %s\n", __FUNCTION__, cnt, final_path);
}


/*
 * read命令的内建函数
 * 使用方式：
 *      read [file_path] [read_size]
 */
void buildin_read(uint32 argc, char **argv) {
    if (argc != 3 || !argv[1]) {
        printk("[%s] unknown operation!\n", __FUNCTION__);
        return;
    }

    const char *size_str = argv[2];
    int read_size = skip_atoi(&size_str);
    uint32 buf_size = read_size + 1; // 最后要有个0结束符
    if (read_size <= 0 || buf_size > PAGE_SIZE) {
        printk("[%s] size illegal!\n", __FUNCTION__);
        return;
    }

    // 处理为绝对路径
    make_clear_abs_path(argv[1], final_path);

    // 打开文件
    int32 fd = sys_open(final_path, O_RDONLY);
    if (fd == -1) {
        printk("[%s] open err !\n", __FUNCTION__);
        return;
    }

    if (sys_seek(fd, 0, SEEK_SET) == -1) {
        printk("[%s] seek err !\n", __FUNCTION__);
        sys_close(fd);
        return;
    }

    // 读取文件内容
    void *buf = kmalloc(buf_size);
    memset(buf, 0, buf_size);
    int cnt = sys_read(fd, buf, read_size);

    // 关闭文件
    if (sys_close(fd) == -1) {
        printk("[%s] close err !\n", __FUNCTION__);
        kmfree_s(buf, buf_size);
        return;
    }

    printk("[%s] ok [%d] -> %s\n", __FUNCTION__, cnt, buf);
    kmfree_s(buf, buf_size);
}

// 内建命令
bool buildin_cmd(uint8 argc, char **argv) {
    if (!argc) return false;

    if (!strcmp(argv[0], "ps")) {
        buildin_ps(argc, argv);
        return true;
    }

    if (!strcmp(argv[0], "ls")) {
        buildin_ls(argc, argv);
        return true;
    }

    if (!strcmp(argv[0], "pwd")) {
        buildin_pwd(argc, argv);
        return true;
    }

    if (!strcmp(argv[0], "cd")) {
        buildin_cd(argc, argv);
        return true;
    }

    if (!strcmp(argv[0], "mkdir")) {
        buildin_mkdir(argc, argv);
        return true;
    }

    if (!strcmp(argv[0], "rmdir")) {
        buildin_rmdir(argc, argv);
        return true;
    }

    if (!strcmp(argv[0], "touch")) {
        buildin_touch(argc, argv);
        return true;
    }

    if (!strcmp(argv[0], "rm")) {
        buildin_rm(argc, argv);
        return true;
    }

    if (!strcmp(argv[0], "write")) {
        buildin_write(argc, argv);
        return true;
    }

    if (!strcmp(argv[0], "read")) {
        buildin_read(argc, argv);
        return true;
    }

    return false;
}
