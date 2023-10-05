//
// Created by toney on 10/4/23.
//

#ifndef TOS_DIR_H
#define TOS_DIR_H

#include "types.h"
#include "chain.h"
#include "ide.h"

void open_root_dir(partition_t *part);

dir_t *dir_open(partition_t *part, uint32 inode_no);

bool search_dir_entry(partition_t *part, dir_t *pdir, const char *name, dir_entry_t *dir_e);

void dir_close(partition_t *part, dir_t *dir);

void create_dir_entry(char *filename, uint32 inode_no, uint8 file_type, dir_entry_t *p_de);

bool sync_dir_entry(dir_t *parent_dir, dir_entry_t *p_de, void *io_buf);

bool delete_dir_entry(partition_t *part, dir_t *pdir, uint32 inode_no, void *io_buf);

dir_entry_t *dir_read(dir_t *dir);

bool dir_is_empty(dir_t *dir);

int32 dir_remove(dir_t *parent_dir, dir_t *child_dir);

#endif //TOS_DIR_H
