//
// Created by toney on 23-9-17.
//

#include "../../include/chain.h"
#include "../../include/print.h"
#include "../../include/sys.h"

// 添加首个元素
void chain_init(chain_t *chain) {
    /*
        |ˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉ|
        |        ↓ˉˉprevˉˉ↑         |
        | NULL | head | tail | NULL |
        |        ↓__next__↑         |
        |___________________________|
    */
    chain->size = 0;
    // 修改head前后项
    chain->head.prev = NULL;
    chain->head.next = &chain->tail;
    chain->head.value = NULL;
    // 修改tail前后项
    chain->tail.prev = &chain->head;
    chain->tail.next = NULL;
    chain->tail.value = NULL;
}

// 在某个元素前插入元素
void chain_insert_before(chain_t *chain, chain_elem_t *target, chain_elem_t *elem) {
    /*
        |ˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉ|
        | +--------------------+  |
        | ↓ˉˉprevˉˉ↑  ↓ˉˉprevˉˉ↑  |
        | prev |   elem  | target |
        | ↓__next__↑  ↓__next__↑  |
        | +--------------------+  |
        |_________________________|
    */
    // 新增项
    elem->prev = target->prev;
    elem->next = target;
    // 目标项的前驱项
    target->prev->next = elem;
    // 目标项
    target->prev = elem;

    chain->size++;
}


// 在某个元素后插入元素
void chain_insert_after(chain_t *chain, chain_elem_t *target, chain_elem_t *elem) {
    /*
        |ˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉ|
        |  +--------------------+ |
        |  ↓ˉˉprevˉˉ↑  ↓ˉˉprevˉˉ↑ |
        | target  | elem   | next |
        |  ↓__next__↑  ↓__next__↑ |
        |  +--------------------+ |
        |_________________________|
    */
    // 新增项
    elem->prev = target;
    elem->next = target->next;
    // 目标项的后驱项
    target->next->prev = elem;
    // 目标项
    target->next = elem;

    chain->size++;
}

// 往链表首部添加元素
void chain_put_first(chain_t *chain, chain_elem_t *elem) {
    chain_insert_before(chain, chain->head.next, elem);
}


// 往链表尾部添加元素
void chain_put_last(chain_t *chain, chain_elem_t *elem) {
    chain_insert_after(chain, chain->tail.prev, elem);
}

// 删除某个元素
chain_elem_t *chain_remove(chain_t *chain, chain_elem_t *elem) {
    /*
        |ˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉ|
        |  ↓ˉˉˉˉˉprevˉˉˉˉ↑    |
        | prev | elem | next  |
        |  ↓_____next____↑    |
        |_____________________|
    */
    if (chain->size == 0 || elem == NULL || elem->prev == NULL || elem->next == NULL) return NULL;
    // 前驱项
    elem->prev->next = elem->next;
    // 后驱项
    elem->next->prev = elem->prev;

    chain->size--;
    return elem;
}

// 通过元素中的value删除某个元素
chain_elem_t *chain_remove_by_value(chain_t *chain, void *value) {
    if (chain->size == 0 || value == NULL) return NULL;
    chain_elem_t *elem = &chain->head;
    for (int i = 0; i < chain->size; ++i) {
        elem = elem->next;
        if (elem->value == value) break;
    }
    if (!elem->value) return NULL;
    return chain_remove(chain, elem);
}

// 弹出链表首部元素
chain_elem_t *chain_pop_first(chain_t *chain) {
    return chain_remove(chain, chain->head.next);
}

// 弹出链表尾部元素
chain_elem_t *chain_pop_last(chain_t *chain) {
    return chain_remove(chain, chain->tail.prev);
}

// 链表中是否存在某个元素
bool chain_exist(chain_t *chain, chain_elem_t *elem) {
    for (chain_elem_t *i = chain->head.next; i != &chain->tail; i = i->next) if (i == elem) return true;
    return false;
}

// 返回链表长度
uint32 chain_len(chain_t *chain) {
    return chain->size;
}

// 返回链表是否为空
bool chain_empty(chain_t *chain) {
    return chain->size == 0 || chain->head.next == &chain->tail;
}

/*
 * 链表缓存池初始化
 */
void chain_pool_init(chain_elem_pool_t *pool) {
    pool->rest = pool->size;
    pool->cursor = pool->addr;
    chain_init(&pool->chain);
}

chain_elem_t *chain_pool_get(chain_elem_pool_t *pool) {
    if (pool->size < sizeof(chain_elem_t)) {
        printk("chain pool is not initialized!\n");
        STOP
        return NULL;
    }
    if (pool->chain.size > 0) return chain_pop_first(&pool->chain);
    if (pool->rest < sizeof(chain_elem_t)) return NULL;
    chain_elem_t *elem = (chain_elem_t *) pool->cursor;
    pool->rest -= sizeof(chain_elem_t);
    pool->cursor += sizeof(chain_elem_t);
    return elem;
}

chain_elem_t *chain_pool_getv(chain_elem_pool_t *pool, void *value) {
    chain_elem_t *elem = chain_pool_get(pool);
    if (elem) elem->value = value;
    return elem;
}

void chain_pool_ret(chain_elem_pool_t *pool, chain_elem_t *elem) {
    chain_put_last(&pool->chain, elem);
}