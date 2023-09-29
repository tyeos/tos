//
// Created by toney on 2023-08-31.
//

#include "../include/stdarg.h"
#include "../include/console.h"
#include "../include/sys.h"
#include "../include/eflags.h"

extern int vsprintf(char *buf, const char *fmt, va_list args);

static char buf[1024];

int printk(const char *fmt, ...) {
    bool iflag = check_close_if();

    // 处理参数并输出信息到控制台
    va_list args;
    int size;
    va_start(args, fmt);
    size = vsprintf(buf, fmt, args);
    va_end(args);
    console_write(buf, size);

    check_recover_if(iflag);
    return size;
}

int sprintfk(char *dest, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int size = vsprintf(dest, fmt, args);
    va_end(args);
    return size;
}