//
// Created by toney on 23-8-26.
//

#include "../include/console.h"
#include "../include/string.h"

void kernel_main(void) {
    console_clear();

    // 打满2屏数据，最后光标换行会顶掉第一行
    char arr1[] = "ABCDEFGHIJK";
    char arr2[] = "0123456789";
    for (int i = 0; i < 50; ++i) {
        char msg[] = {arr1[i/10], arr2[i % 10], '\n', 0};
        console_write(msg, strlen(msg));
    }

    // 定位屏幕到首行
    scroll_to_top();

    // 再打印一整行数据，换行会触发定位屏幕到光标处
    char msg[80];
    for (int i = 0; i < 80; ++i) {
        msg[i] = 'C';
    }
    msg[80] = 0;
    console_write(msg, strlen(msg));
}
