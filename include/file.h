//
// Created by toney on 10/4/23.
//

#ifndef TOS_FILE_H
#define TOS_FILE_H

#include "types.h"
#include "chain.h"
#include "ide.h"

uint32 get_free_slot_in_global();

int32 pcb_fd_install(int32 global_fd_idx);

uint32 inode_bitmap_alloc(partition_t *part);

uint32 block_bitmap_alloc(partition_t *part);

void bitmap_sync(partition_t *part, uint32 bit_idx, enum bitmap_type btmp);

int32 file_create(dir_t *parent_dir, char *filename);

#endif //TOS_FILE_H
