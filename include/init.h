//
// Created by toney on 2023-09-02.
//

#ifndef TOS_INIT_H
#define TOS_INIT_H

void physical_memory_init();
void virtual_memory_init();

void gdt_init();
void idt_init();

void clock_init();

#endif //TOS_INIT_H
