//
// Created by toney on 23-9-2.
//

#include "../../include/io.h"
#include "../../include/clock.h"

void clock_init() {
    outb(PIT_CTRL_REG, 0b00110100);
    outb(PIT_CHAN0_REG, CLOCK_COUNTER & 0xff);
    outb(PIT_CHAN0_REG, (CLOCK_COUNTER >> 8) & 0xff);
}
