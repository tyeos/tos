//
// Created by toney on 2023-09-02.
//

#ifndef TOS_SYS_H
#define TOS_SYS_H

#define BOCHS_DEBUG_MAGIC __asm__("xchg bx, bx");

#define CLI __asm__("cli"); // 设置IF=0，即关中断 (不再响应任何可屏蔽中断)
#define STI __asm__("sti"); // 设置IF=1，即开中断 (允许响应可屏蔽中断)

#endif //TOS_SYS_H
