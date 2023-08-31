//
// Created by toney on 23-8-30.
//

#ifndef TOS_CONSOLE_H
#define TOS_CONSOLE_H

#include "../include/linux/types.h"

void console_clear(void);
void console_write(char *buf, uint32 count);

void scroll_to_top();
void scroll_to_cursor();

#endif //TOS_CONSOLE_H
