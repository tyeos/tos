//
// Created by toney on 2023-09-08.
//

#include "../../include/mm.h"
#include "../../include/print.h"

#define ARDS_TIMES_ADDR 0x1100
#define ARDS_BUFFER_ADDR 0x1102

checked_memory_info_t info;

void print_checked_memory_info() {
    printk("checked_memory_info_t size = %d\n", sizeof(checked_memory_info_t));
    info.times = *(uint16 *) ARDS_TIMES_ADDR;
    info.ards = (checked_memory_info_t *) ARDS_BUFFER_ADDR;

    for (int i = 0; i < info.times; i++) {
        ards_t *p = info.ards + i;

        uint64 toAddr = ((uint64) (p->base_addr_high) << 32) + (p->base_addr_low) +
                        ((uint64) (p->length_high) << 32) + (p->length_low) - 1;
        printk("Addr=[0x%x%08x ~ 0x%x%08x], Len=0x%x%08x, Type=%d\n", p->base_addr_high, p->base_addr_low,
               (uint32) (toAddr >> 32), (uint32) toAddr, p->length_high, p->length_low, p->type);
    }
}