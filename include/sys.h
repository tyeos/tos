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
/*
 * 注：现在SLEEP函数实现比较简单，只是将时间转成对应的CPU滴答数，然后释放指定次数个执行周期，计时并没有很精确，
 *      这个休眠还是会让任务获取到执行权限的，只是如果次数没到，就立即释放权限，直至到达指定次数，
 *      而且还要考虑一个调度任务数的问题，就比如SLEEP_ITS中每次自增应该是加当前任务数更好一些，目前只是单任务好一些，
 *      所以如果优化，可以在任务调度中，将正在休眠的任务跳过，等该任务到达指定数量个时钟周期，在恢复其调度即可，
 *      现在已经有休眠效果了，这个就不做具体实现了，只是个功夫活儿，知晓原理即可。
 */
#define SLEEP_MS(millisecond) SLEEP_ITS(MS2TIMES(millisecond))      // 暂停指定毫秒数
#define SLEEP(second) SLEEP_ITS(S2TIMES(second))                    // 暂停指定秒数

#endif //TOS_SYS_H
