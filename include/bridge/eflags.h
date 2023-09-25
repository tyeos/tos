//
// Created by toney on 23-9-9.
//

#ifndef TOS_EFLAGS_H
#define TOS_EFLAGS_H

#include "../types.h"

uint32 get_eflags();

void set_eflags(uint32);

uint8 get_if_flag();

#define check_close_if() ({ \
    bool ack_int = get_if_flag(); \
    if (ack_int) CLI       \
    ack_int;\
})

#define check_recover_if(before_ack_int) ({if (before_ack_int) STI})

#endif //TOS_EFLAGS_H
