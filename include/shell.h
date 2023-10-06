//
// Created by toney on 10/5/23.
//

#ifndef TOS_SHELL_H
#define TOS_SHELL_H

#include "types.h"

#define CMD_LEN 128     // 支持的最大命令行字符数
#define MAX_ARG_NR 16   // 加上命令名外，最多支持15个参数
#define CWD_SIZE 64     // 当前工作目录缓存大小

enum cmd_model {
    MODEL_OUTPUT, // 常规输出模式
    MODEL_CMD     // 输入命令模式
};

void check_exec_shell();

void keyboard_input(bool ctl, char c);

bool buildin_cmd(uint8 argc, char **argv);

#endif //TOS_SHELL_H
