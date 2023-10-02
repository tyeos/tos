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
    void *value;
} __attribute__((packed)) chain_elem_t;

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
} __attribute__((packed)) chain_t;

/*
 * 链表元素池，从这里申请链表元素，
 * 对于需要频繁增删链表元素的场景，用此结构可避免频繁回收
 */
typedef struct chain_elem_pool_t {
    void *addr;            // 可分配的起始地址
    uint32 size;           // 占用空间字节数
    /* 以上初始化用，以下运行用 */
    char *cursor;          // 地址分配记录
    uint32 rest;           // 剩余待分配的地址大小
    chain_t chain;         // 已启用的缓存
} __attribute__((packed)) chain_elem_pool_t;

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

// 读取链表中的第一个元素
chain_elem_t *chain_read_first(chain_t *chain);

// 读取链表中某个元素的的下一个元素
chain_elem_t *chain_read_next(chain_t *chain, chain_elem_t *cur_elem);

// 删除某个元素
chain_elem_t *chain_remove(chain_t *chain, chain_elem_t *elem);

// 通过元素中的value删除某个元素
chain_elem_t *chain_remove_by_value(chain_t *chain, void *value);

// 链表中是否存在某个元素
bool chain_exist(chain_t *chain, chain_elem_t *elem);

// 返回链表长度
uint32 chain_len(chain_t *chain);

// 返回链表是否为空
bool chain_empty(chain_t *chain);

// 初始化链表元素缓存池
void chain_pool_init(chain_elem_pool_t *pool);

// 从缓存池中获取一个链表元素
chain_elem_t *chain_pool_get(chain_elem_pool_t *pool);

// 从缓存池中获取一个链表元素，并自动赋值
chain_elem_t *chain_pool_getv(chain_elem_pool_t *pool, void *value);

// 将链表元素归还给缓存池
void chain_pool_ret(chain_elem_pool_t *pool, chain_elem_t *elem);

#endif //TOS_CHAIN_H
