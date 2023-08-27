//
// Created by toney on 23-8-26.
//

void kernel_main(void) {
    // 在屏幕第三行首个位置输出字符
    char* video = (char*)(0xb8000+160*2);
    *video = 'C';
}
