//
// Created by toney on 23-8-30.
//

#ifndef TOS_IO_H
#define TOS_IO_H

char inb(int port);
short inw(int port);

void outb(int port, int value);
void outw(int port, int value);

#endif //TOS_IO_H
