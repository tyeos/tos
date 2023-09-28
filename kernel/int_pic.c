//
// Created by toney on 2023-09-03.
//

#include "../include/io.h"
#include "../include/print.h"
#include "../include/int.h"

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

void clock_interrupt_handler() {
    // 该函数已废弃
    printk("clock interrupt!\n");
}

// 可编程中断控制器（8259A）的中断处理
void interrupt_handler_pic(int vector_no, int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax,
                           int eip,int cs, int eflags, int esp_r3, int ss_r3) {
    send_eoi(vector_no);
    switch (vector_no) {
        case 0x20:// 时钟中断
            clock_interrupt_handler();
            break;
        case 0x21:// 键盘中断
            keyboard_interrupt_handler();
            break;
        default:  // 其他中断
            printk("\npic interrupt: VECTOR = 0x%02X\n", vector_no);
            break;
    }
}
