//
// Created by Toney on 2023/10/6.
//

#include "../../include/print.h"
#include "../../include/fs.h"
#include "../../include/string.h"
#include "../../include/sys.h"
#include "../../include/types.h"
#include "../../include/console.h"
#include "../../include/shell.h"
#include "../../include/task.h"


static enum cmd_model cur_model = MODEL_OUTPUT;   // 当前命令模式

static char cmd_line_writing[CMD_LEN] = {0};        // 存储正在输入的命令
static char cmd_line_ready[CMD_LEN] = {0};          // 存储就绪的命令
static char cmd_line_running[CMD_LEN] = {0};        // 存储正在运行的命令

static char *cmd_pos = NULL;        // 临时记录正在输入的命令字符位置
static char *cmd_argv[MAX_ARG_NR];  // 存放正在运行的命令 解析出的参数列表

char cwd_cache[CWD_SIZE] = {0};     // 当前工作目录缓存，每次执行cd命令时会更新此内容


/**
 * 将字符串以指定分隔符分割
 * @param cmd_str 分割哪个字符串
 * @param argv 存放分割结果的指针
 * @param token 分隔符
 * @return 解析出的参数数量
 */
static uint8 cmd_parse(char *cmd_str, char **argv, char token) {
    for (uint8 arg_idx = 0; arg_idx < MAX_ARG_NR; ++arg_idx) argv[arg_idx] = NULL; // 先将将结果集置空
    char *next = cmd_str;                       // 存放扫描的字符
    uint8 argc = 0;                             // 扫描出的参数个数
    while (*next) {                             // 外层循环处理整个命令行
        while (*next == token) next++;          // 读参数前先去除无效字符
        if (*next == EOS) break;                // 读到最后返回
        argv[argc] = next;                      // 读到有效字符进行记录
        while (*next && *next != token) next++; // 读到EOS或下一个分隔符处截止
        if (*next) *next++ = EOS;               // 如果不是读到EOS结束手动将分隔符换为EOS
        if (argc > MAX_ARG_NR) return 0;        // 避免argv数组访问越界，参数过多则返回0
        argc++;                                 // 处理完一个参数+1
    }
    return argc;
}

// 输出提示符
static void print_prompt() {
    printk("\n[toney@localhost %s]$ ", cwd_cache);
}

// 切换命令模式
static void switch_cmd_model() {
    cur_model = cur_model == MODEL_OUTPUT ? MODEL_CMD : MODEL_OUTPUT;
    if (cur_model == MODEL_CMD) {
        printk("\n--- open cmd ---\n");

        // 如果初始没有值，默认指向根目录
        if (!cwd_cache[0]) cwd_cache[0] = '/';

        // 清空命令行缓存
        memset(cmd_line_writing, 0, CWD_SIZE);
        cmd_pos = cmd_line_writing;

        // 输出提示符
        print_prompt();
    } else {
        printk("\n--- close cmd ---\n");
    }
}

static void ready_cmd() {
    // 如果没输入任何内容，换行输出命令行提示符即可
    if (!cmd_line_writing[0]) {
        print_prompt();
        return;
    }

    // 如果上一条命令还没执行完，这行命令就暂不执行
    if (cmd_line_ready[0]) {
        // 输出提示，并重新将命令输出到屏幕
        printk("[%s] last task is currently executing ~\n", __FUNCTION__);
        print_prompt();
        printk("%s", cmd_line_writing);
        return;
    }

    // 将命令复制到就绪队列
    memcpy(cmd_line_ready, cmd_line_writing, CMD_LEN);

    // 还原命令行窗口, 等待处理结果
    memset(cmd_line_writing, 0, CWD_SIZE);
    cmd_pos = cmd_line_writing;
}

static void input_cmd_char(char ch) {
    switch (ch) {
        // 找到回车或换行符后认为键入的命令结束，直接返回
        case '\n':
        case '\r':
            // 换行符正常输出
            printk("%c", ch);
            // 执行指令
            *cmd_pos = EOS;
            ready_cmd();
            return;
        case '\b':
            // 删到第一个字符就别删了
            if (cmd_pos == cmd_line_writing) return;
            // 将字符从屏幕中删除
            printk("%c", ch);
            *cmd_pos = EOS;
            cmd_pos--;
            return;
        default:
            break;
    }

    // 命令行字符数不可超出限制
    if (cmd_pos >= cmd_line_writing + CMD_LEN) return;

    // 将字符输出到屏幕
    printk("%c", ch);

    *cmd_pos = ch;
    cmd_pos++;
}

void shortcut_cmd(char ch) {
    switch (ch) {
        case 's': // ctrl + s: 切换模式 （switch）
            switch_cmd_model();
            return;
        case 'f': // ctrl + f: 向下翻页 （forward）
            scroll_up_page();
            return;
        case 'b': // ctrl + b: 向上翻页 （backward）
            scroll_down_page();
            return;
        case 'd': // ctrl + d: 向下翻半页 （down）
            scroll_up_rows(12); // 屏幕上卷
            return;
        case 'u': // ctrl + u: 向上翻半页 （up）
            scroll_down_rows(12); // 屏幕下卷
            return;
        default:
            break;
    }
}

void keyboard_input(bool ctl, char ch) {
    // 快捷键处理
    if (ctl) {
        shortcut_cmd(ch);
        return;
    }

    // 非命令模式不向屏幕输出字符
    if (cur_model != MODEL_CMD) return;

    // 命令字符
    input_cmd_char(ch);
}

void check_exec_shell() {
    // 如果没有就绪的命令，释放控制权，继续等待
    if (!cmd_line_ready[0]) return;

    // 取出命令
    memcpy(cmd_line_running, cmd_line_ready, CMD_LEN);
    memset(cmd_line_ready, 0, CMD_LEN);

    // 正常解析执行命令
    printk("\n");
    if (cmd_line_running[0]) {
        uint8 cmd_argc = cmd_parse(cmd_line_running, cmd_argv, ' ');
        if (!buildin_cmd(cmd_argc, cmd_argv)) {
            printk("[%s] not supported: %s\n", __FUNCTION__, cmd_line_running);
        }
    }

    // 处理完成，输出命令行提示, 如果已经输出了一部分命令，则追加其后
    print_prompt();
    if (cmd_line_writing[0]) printk("%s", cmd_line_writing);
}