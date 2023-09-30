//
// Created by toney on 23-9-2.
//

#ifndef TOS_PRINT_H
#define TOS_PRINT_H

#include "../include/types.h"

int printk(const char *fmt, ...);

int sprintfk(char *dest, const char *fmt, ...);

void print_hex_buff(const char *buff, uint32 size);

#endif //TOS_PRINT_H
