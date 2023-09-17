//
// Created by toney on 23-9-17.
//

#ifndef TOS_CHAIN_H
#define TOS_CHAIN_H

#include "types.h"

// 双向链表
typedef struct chain_t {
    void *value;
    struct chain_t *previous;
    struct chain_t *next;
} chain_t;

// 双向环形链表
typedef struct circular_chain_t {
    size_t size; // 正在使用的链表长度
    chain_t *current; // 正在使用的链表
} circular_chain_t;

// 添加item到最后一个(即上一个)位置, 并返回添加成功的元素
chain_t *put_last(circular_chain_t* list, chain_t *item);

// 读下一个item, 并将cursor后移
chain_t *next_item(circular_chain_t *list);

// 读当前item, 并将cursor后移
chain_t *read_item(circular_chain_t *list);

// 删除item, 并返回删除成功的元素
chain_t *remove_item(circular_chain_t *list, chain_t *item) ;


#endif //TOS_CHAIN_H
