//
// Created by Toney on 2023/10/6.
//

#include "../include/print.h"
#include "../include/fs.h"
#include "../include/string.h"
#include "../include/sys.h"
#include "../include/types.h"
#include "../include/console.h"


#define cmd_len 128     // 支持的最大命令行字符数
#define MAX_ARG_NR 16   // 加上命令名外，最多支持15个参数
#define cwd_size 64


enum cmd_model {
    OUTPUT, // 输出模式
    CMD     // 输入命令模式
};

static enum cmd_model cur_model = OUTPUT;   // 当前命令模式

static char cwd_cache[cwd_size] = {0};       // 用来记录当前目录，是当前目录的缓存，每次执行cd命令时会更新此内容

static char cmd_line[cmd_len] = {0};        // 存储输入的命令
static char *cmd_pos = NULL;                // 临时记录命令字符


// 输出提示符
static void print_prompt() {
    printk("[toney@localhost %s]$ ", cwd_cache);
}

// 切换命令模式
static void switch_cmd_model() {
    cur_model = cur_model == OUTPUT ? CMD : OUTPUT;
    if (cur_model == CMD) {
        printk("\n--- open cmd ---\n");

        // 默认指向根目录
        if (!cwd_cache[0]) cwd_cache[0] = '/';

        // 清空命令行缓存
        memset(cmd_line, 0, cwd_size);
        cmd_pos = cmd_line;

        // 输出提示符
        print_prompt();
    } else {
        printk("\n--- close cmd ---\n");
    }

}

static void exec_cmd() {
    printk("\n[%s] not supported: %s\n", __FUNCTION__, cmd_line);

    memset(cmd_line, 0, cwd_size);
    cmd_pos = cmd_line;

    print_prompt();
}

static void input_cmd_char(char ch) {
    switch (ch) {
        // 找到回车或换行符后认为键入的命令结束，直接返回
        case '\n':
        case '\r':
            *cmd_pos = EOS;
            exec_cmd();
            return;
        case '\b':
            // 删到第一个字符就别删了
            if (cmd_pos == cmd_line) return;
            // 将字符从屏幕中删除
            printk("%c", ch);
            *cmd_pos = EOS;
            cmd_pos--;
            return;
        default:
            break;
    }

    // 命令行字符数不可超出限制
    if (cmd_pos >= cmd_line + cmd_len) return;

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

    // 输出模式不向屏幕输出字符
    if (cur_model == OUTPUT) {
        return;
    }

    // 命令字符
    input_cmd_char(ch);
}
