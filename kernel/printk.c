//
// Created by toney on 2023-08-31.
//

#include "../include/stdarg.h"
#include "../include/console.h"

static char buf[1024];

extern int vsprintf(char *buf, const char *fmt, va_list args);

int printk(const char * fmt, ...) {
    va_list args;
    int size;

    va_start(args, fmt);
    size = vsprintf(buf, fmt, args);
    va_end(args);

    console_write(buf, size);
    return size;
}