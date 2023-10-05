//
// Created by toney on 10/4/23.
//

#ifndef TOS_FILE_H
#define TOS_FILE_H

#include "types.h"
#include "chain.h"
#include "ide.h"

int32 get_free_slot_in_global();

int32 pcb_fd_install(int32 global_fd_idx);

uint32 inode_bitmap_alloc(partition_t *part);

uint32 block_bitmap_alloc(partition_t *part);

void bitmap_sync(partition_t *part, uint32 bit_idx, enum bitmap_type btmp);

int32 file_create(dir_t *parent_dir, char *filename);

int32 file_open(uint32 inode_no, uint8 flag);

int32 file_write(file_t *file, const void *buf, uint32 count);

int32 file_read(file_t *file, void *buf, uint32 count);

int32 file_close(file_t *file);

#endif //TOS_FILE_H
