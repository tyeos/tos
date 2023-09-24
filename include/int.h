//
// Created by toney on 23-9-2.
//

#ifndef TOS_PIC_HANDLER_H
#define TOS_PIC_HANDLER_H

#include "types.h"

void interrupt_handler_pic(int vector_no, int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax,
                           int eip, int cs, int eflags, int esp_r3, int ss_r3);
void interrupt_handler_exception(int vector_no, int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax,
                                 int err_code, int eip, int cs, int eflags, int esp_r3, int ss_r3);
bool interrupt_has_error_code(int vector_no);

void keyboard_interrupt_handler();
void clock_interrupt_handler();

#endif //TOS_PIC_HANDLER_H
