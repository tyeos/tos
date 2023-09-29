//
// Created by toney on 23-8-30.
//

#ifndef TOS_IO_H
#define TOS_IO_H

#include "types.h"

// 单字节读入
char inb(int port);
// 单字读入
short inw(int port);

// 单字节写出
void outb(int port, uint32 value);
// 单字写出
void outw(int port, uint32 value);

// 按字读入nr次
#define insw(port, buf, nr) __asm__("cld;rep;insw"::"d"(port),"D"(buf),"c"(nr))
// 按字写出nr次
#define outsw(port, buf, nr) __asm__("cld;rep;outsw"::"d"(port),"S"(buf),"c"(nr))

#endif //TOS_IO_H
