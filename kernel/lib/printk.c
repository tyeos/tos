//
// Created by toney on 2023-08-31.
//

#include "../../include/stdarg.h"
#include "../../include/console.h"
#include "../../include/sys.h"
#include "../../include/eflags.h"
#include "../../include/print.h"

extern int vsprintf(char *buf, const char *fmt, va_list args);

static char print_buff[1024];

int printk(const char *fmt, ...) {
    bool iflag = check_close_if();

    // 处理参数并输出信息到控制台
    va_list args;
    int size;
    va_start(args, fmt);
    size = vsprintf(print_buff, fmt, args);
    va_end(args);
    console_write(print_buff, size);

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


void print_hex_buff(const char *buff, uint32 size) {
    const uint8 line_max = 0x10; // 单行最大显示16字节
    for (uint32 start = 0, compress = 0, line = line_max; start < size; start += line_max) {
        // 最后一行特殊处理
        if (start + line > size) line = size - start;

        // 标准行且不是最后一行，则检查本行是否全是0，是则开始压缩
        if (line == line_max && start + line != size &&
            !(*(uint64 *) (buff + start) | *(uint64 *) (buff + start + 8))) {
            compress++; // 压缩行数+1
            continue;
        }

        // 这一行不需要压缩，处理之前的压缩行
        if (compress) {
            printk("0x%04x~0x%04x: 0x00 ... (compress line 0x%x)\n", start - compress * line_max, start - 1, compress);
            compress = 0;
        }

        // 当前行
        printk("0x%04x~0x%04x: ", start, start + line);
        for (int i = 0; i < line; ++i) {
            printk("%02x ", buff[start + i] & 0xff);
        }
        printk("\n");
    }
}
