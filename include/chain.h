//
// Created by toney on 23-9-17.
//

#ifndef TOS_CHAIN_H
#define TOS_CHAIN_H

#include "types.h"

// 链表元素
typedef struct chain_elem_t {
    struct chain_elem_t *prev; // previous 前驱节点
    struct chain_elem_t *next; // 后继节点
} chain_elem_t;

/*
 链表结构:
              +--------------------size-------------------+
     head <=> | elem_0 <=> elem_1 <=> elem_... <=> elem_n | <=> tail
              +-------------------------------------------+
 */
typedef struct chain_t {
    chain_elem_t head; // 头部节点
    chain_elem_t tail; // 尾部节点
    uint32 size;       // 正在使用的链表长度
} chain_t;

// 初始化链表
void chain_init(chain_t *chain);

// 在某个元素前插入元素
void chain_insert_before(chain_t *chain, chain_elem_t *target, chain_elem_t *elem);

// 在某个元素后插入元素
void chain_insert_after(chain_t *chain, chain_elem_t *target, chain_elem_t *elem);

// 往链表首部添加元素
void chain_put_first(chain_t *chain, chain_elem_t *elem);

// 往链表尾部添加元素
void chain_put_last(chain_t *chain, chain_elem_t *elem);

// 弹出链表首部元素
chain_elem_t *chain_pop_first(chain_t *chain);

// 弹出链表尾部元素
chain_elem_t *chain_pop_last(chain_t *chain);

// 删除某个元素
chain_elem_t *chain_remove(chain_t *chain, chain_elem_t *elem);

// 链表中是否存在某个元素
bool chain_exist(chain_t *chain, chain_elem_t *elem);

// 返回链表长度
uint32 chain_len(chain_t *chain);

// 返回链表是否为空
bool chain_empty(chain_t *chain);

#endif //TOS_CHAIN_H
