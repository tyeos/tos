//
// Created by toney on 23-8-26.
//

#include "../bridge/io.h"

void kernel_main(void) {
    // 在屏幕第三行首个位置输出字符
    char *video = (char *) (0xb8000 + 160 * 2);
    *video = 'C';

    // 设置光标至字符后面一个位置
    unsigned int index = 160 * 2 + 2;
    outb(0x3D4, 0xE);
    outb(0x3D5, (index >> 9) & 0xff);
    outb(0x3D4, 0xF);
    outb(0x3D5, (index >> 1) & 0xff);
}
