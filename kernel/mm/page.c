//
// Created by toney on 2023-09-09.
//

//由于使用的inline函数, 所以一定要include对应的方法声明
#include "../../include/mm.h"

inline uint get_cr3() {
    asm volatile("mov eax, cr3;");
}

inline void set_cr3(uint v) {
    asm volatile("mov cr3, eax;"::"a"(v));
}

inline void enable_page() {
    // 打开CR0的PG位（第31位）
    asm volatile("mov eax, cr0;"
                 "or eax, 0x80000000;"
                 "mov cr0, eax;");
}