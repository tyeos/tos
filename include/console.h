//
// Created by toney on 23-8-30.
//

#ifndef TOS_CONSOLE_H
#define TOS_CONSOLE_H

#include "../include/types.h"

void console_clear(void);
void console_write(char *buf, uint32 count);

void scroll_to_top();
void scroll_to_cursor();

void scroll_up_page();
void scroll_down_page();
int scroll_up_rows(int rows);
int scroll_down_rows(int rows);

#endif //TOS_CONSOLE_H
