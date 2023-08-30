//
// Created by toney on 23-8-26.
//

#include "../include/console.h"
#include "../include/string.h"

void kernel_main(void) {
    console_clear();
    char* msg ="Hi\nC!\n";
    console_write(msg, strlen(msg));
}
