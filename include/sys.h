//
// Created by toney on 2023-09-02.
//

#ifndef TOS_SYS_H
#define TOS_SYS_H

#include "clock.h"

#define BOCHS_DEBUG_MAGIC __asm__("xchg bx, bx");

#define CLI __asm__("cli"); // 设置IF=0，即关中断 (不再响应任何可屏蔽中断)
#define STI __asm__("sti"); // 设置IF=1，即开中断 (允许响应可屏蔽中断)

/*
 * 处理器暂停(Halt)指令, 不影响标志。
 * 当复位线上有复位信号(RESET)、CPU响应非屏蔽中断(NMI)、
 * CPU响应可屏蔽中断(MSI) 3种情况之一时，CPU脱离暂停状态，执行HLT的下一条指令。
 * 如果hlt指令之前, 做了cli, 那可屏蔽中断不能唤醒cpu。
 */
#define HLT __asm__("hlt");

#define STOP __asm__("cli; hlt;"); // 关中断并释放CPU，即让程序停止执行

#define SLEEP_ITS(times) for (uint32 _i = 0; _i < times; ++_i) HLT  // sleep interrupt times, 暂停指定次数个时钟中断
#define SLEEP_MS(millisecond) SLEEP_ITS(MS2TIMES(millisecond))      // 暂停指定毫秒数
#define SLEEP(second) SLEEP_ITS(S2TIMES(second))               // 暂停指定秒数

#endif //TOS_SYS_H
