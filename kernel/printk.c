//
// Created by toney on 2023-08-31.
//

#include "../include/stdarg.h"
#include "../include/console.h"
#include "../include/sys.h"
#include "../include/bridge/eflags.h"

static char buf[1024];

extern int vsprintf(char *buf, const char *fmt, va_list args);

int printk(const char * fmt, ...) {
    // 如果响应中断, 则临时关闭
    uint32 flags = get_eflags();
    bool ack_int = flags & 0x200;
    if (ack_int) {
        CLI
    }

    // 处理参数并输出信息到控制台
    va_list args;
    int size;
    va_start(args, fmt);
    size = vsprintf(buf, fmt, args);
    va_end(args);
    console_write(buf, size);

    // 如果之前响应中断, 则恢复
    if (ack_int) {
        STI
    }
    return size;
}