//
// Created by Toney on 2023/9/28.
//

#ifndef TOS_CLOCK_H
#define TOS_CLOCK_H

/*
  ----------------------------------------------------------------------------------------------------------------------
  可编程定时计数器8253说明
  ----------------------------------------------------------------------------------------------------------------------
  在8253内部有3个独立的计数器（计数器0~计数器02）：
      计数器又称为通道，每个计数器都完全相同，都是16位大小。
      三个计数器可以同时工作，互不影响，各个计数器都有自己的一套寄存器资源。
      寄存器资源包括一个16位的计数初值寄存器、一个计数器执行部件和一个输出锁存器。
      其中，计数器执行部件是计数器中真正进行计数工作的元器件，其本质是个减法计数器。
      每个计数器都有三个引脚：CLK,GATE,OUT：
      CLK:
          表示时钟输入信号，即计数器自己工作的节拍，也就是计数器自己的时钟频率。
          每当此引脚收到一个时钟信号，减法计数器就将计数值减1。
          连接到此引脚的脉冲频率最高为10MHz, 8253为2MHz。
      GATE：
          表示门控输入信号，在某些工作方式下用于控制计数器是否可以开始计数，在不同工作方式下GATE的作用不同。
      OUT:
          表示计数器输出信号。当定时工作结束，也就是计数值为0时，根据计数器的工作方式，会在OUT引脚上输出相应的信号。
          此信号用来通知处理器或某个设备：定时完成。这样处理器或外部设备便可以执行相应的行为动作。

  ----------------------------------------------------------------------------------------------------------------------
  计数器0 （端口 0x40）：
      在个人计算机中，计数器0专用于产生实时时钟信号。
      它采用工作方式3，往此计数器写入0时则为最大计数值65536
  计数器1 （端口 0x41）：
      在个人计算机中，计数器1专用于DRAM的定时刷新控制。
      PC/XT规定在2ms内进行128次的刷新，PC/AT规定在4ms内进行256次的刷新
  计数器2 （端口 0x42）：
      在个人计算机中，计数器2专用于内部扬声器发出不同音调的声音，原理是给扬声器输送不同频率的方波

  ----------------------------------------------------------------------------------------------------------------------
  8253控制字格式:
       -------------------------------------------------
       |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
       | SC1 | SC0 | RW1 | RW0 |  M2 |  M1 |  M0 | BCD |
       -------------------------------------------------
  SC1、SC0：
      选择计数器位，即Select Counter, 或者叫选择通道位，即Select Channel.
      这两位可组合4个寄存器名称，二进制00b~10b分别对应计数器0~计数器2, 11b未定义。
  RW1、RW0：
      位是读/写/锁存操作位，即Read/Write/Latch,用来设置待操作计数器（通道）的读写及锁存方式。
      00b表示锁存数据供cpu读,01b表示只读写低字节,10b表示只读写高字节,11b表示先读写低字节
  M2~M0:
      这三位是工作方式（模式）选择位，即Method或Mode。每个计数器有6种不同的工作方式，即方式0~方式5。
      方式0：计数结束中断方式(Interrupt on Terminal Count)
      方式1：硬件可重触发单稳方式(Hardware Retriggerable One-Shot)
      方式2：比率发生器(Rate Generator)
      方式3：方波发生器(Square Wave Generator)
      方式4：软件触发选通(Software Triggered Strobe)
      方式5：硬件触发选通(Hardware Triggered Strobe)
  BCD:
      数制位，用来指示计数器的计数方式是BCD码（十进制），还是二进制数。
      当BCD位为1时，则表示用BCD码来计数，BCD码的初始值范围是0~0x9999（即十进制范围是0~9999，0表示十进制1000）
      当BCD位为0时，则表示用二进制数来计数，即对应十进制范围是0~65535, 0值表示65536。

  ----------------------------------------------------------------------------------------------------------------------
  并不是所有的工作方式都能让计数器周期性地发出中断信号，假设计数器0工作在方式2下，中断信号产生原理：
      CLK引脚上的时钟脉冲信号是计数器的工作频率节拍，三个计数器的工作频率均是1.19318MHz,即一秒内会有1193180次脉冲信号。
      每发生一次时钟脉冲信号，计数器就会将计数值减1，也就是1秒内会将计数值减1193180次。
      当计数值递减为0时，计数器就会通过OUT引脚发出一个输出信号，此输出信号用于向处理器发出时钟中断信号。
      一秒内会发出多少个输出信号，取决于计数值变成0的速度，也就是取决于计数初始值是多少。
      默认情况下计数器0的初值寄存器值是0，即表示65536。计数值从65536变成0需要修改65536次，
      所以，一秒内发输出信号的次数为1193180/65536，约等于18.206，即一秒内发的输出信号次数为18.206次，时钟中断信号的频率为18.206Hz。
      1000毫秒/(1193180/65536)约等于54.925,这样相当于每隔55毫秒就发一次中断。
      即 1193180/计数器0的初始计数值=中断信号的频率，所以 1193180/中断信号的频率=计数器0的初始计数值

  ----------------------------------------------------------------------------------------------------------------------
 */

#define PIT_CHAN0_REG 0X40 // 计数器0的端口
#define PIT_CTRL_REG 0X43  // 控制字的端口

#define HZ 19 // 每秒中断次数，即中断频率, 最大值为1193182, 由于计数器宽度为16位, 所以按以下算法HZ可用的最小值为 (1193182+65535)>>16, 即19(对应CLOCK_COUNTER为62799)
#define CLOCK_TICK_RATE 1193182 // 时钟每秒脉冲个数。若要1秒中断HZ次, 则每个HZ需要 CLOCK_TICK_RATE/HZ 个时钟
#define CLOCK_COUNTER ((CLOCK_TICK_RATE + HZ/2) / HZ) // 先加HZ/2是为取更高的精度, CLOCK_COUNTER最小值为1(中断频率最高), 最大值为65536(用0时表示,中断频率最低)

#define MS2TIMES(ms) (ms * HZ / 1000) // millisecond to times: 毫秒转中断次数
#define S2TIMES(s) (s * HZ) // second to times: 秒转中断次数

#endif //TOS_CLOCK_H
