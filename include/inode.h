//
// Created by toney on 10/4/23.
//

#ifndef TOS_INODE_H
#define TOS_INODE_H

#include "types.h"
#include "chain.h"
#include "ide.h"

void inode_sync(partition_t *part, inode_t *inode, void *io_buf);

inode_t *inode_open(partition_t *part, uint32 inode_no);

void inode_close(partition_t *part, inode_t *inode);

void inode_init(uint32 inode_no, inode_t *new_inode);

void inode_release(partition_t *part, uint32 inode_no);

#endif //TOS_INODE_H
