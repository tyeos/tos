//
// Created by toney on 2023-09-03.
//

#include "../include/bridge/io.h"
#include "../include/linux/kernel.h"
#include "../include/keyboard.h"

/*
 * PIC, 即 Programmable Interrupt Controller
 * 这里处理的主要是8259a芯片
 */
#define PIC_M_CTRL  0x20    // 主片的控制端口
#define PIC_M_DATA  0x21    // 主片的数据端口
#define PIC_S_CTRL  0xa0    // 从片的控制端口
#define PIC_S_DATA  0xa1    // 从片的数据端口
#define PIC_EOI     0x20    // 通知中断控制器中断结束

// 由于ICW4的AEOI位已设为1，即自动结束中断(Auto End Of Interrupt)，所以不用再单独发送EOI，即此方法可以不使用
static void send_eoi(int idt_index) {
    if (idt_index >= 0x20 && idt_index < 0x30) { // 由8259A芯片触发
        outb(PIC_M_CTRL, PIC_EOI);
        if (idt_index >= 0x28) { // 由8259A从片触发
            outb(PIC_S_CTRL, PIC_EOI);
        }
    }
}

// 可编程中断控制器（8259A）的中断处理
void interrupt_handler_pic(int idt_index, int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax, int eip,char cs, int eflags) {
    // 键盘中断
    if (idt_index == 0x21) {
        keyboard_interrupt_handler();
        return;
    }

    // 其他中断
    printk("\npic interrupt: VECTOR = 0x%02X\n", idt_index);
}
