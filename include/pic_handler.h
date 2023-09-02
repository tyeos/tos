//
// Created by toney on 23-9-2.
//

#ifndef TOS_PIC_HANDLER_H
#define TOS_PIC_HANDLER_H

void interrupt_handler_pic(int idt_index, int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax, int eip,char cs, int eflags);

#endif //TOS_PIC_HANDLER_H
