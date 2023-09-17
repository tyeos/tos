//
// Created by toney on 23-9-17.
//

#include "../include/chain.h"

chain_t *put_last(circular_chain_t *list, chain_t *item) {
    if (list->size == 0) {
        list->current = item;
        item->next = item;
        item->previous = item;
        list->size = 1;
        return item;
    }
    // 上一个item的后面
    list->current->previous->next = item;
    // 新加入的item前后
    item->previous = list->current->previous;
    item->next = list->current;
    // 当前item的前面
    list->current->previous = item;

    list->size++;
    return item;
}

// 读下一个item, 并将cursor后移
chain_t *next_item(circular_chain_t *list) {
    if (list->current == NULL) {
        return NULL;
    }
    chain_t *item = list->current->next;
    list->current = item;
    return item;
}

// 读当前item, 并将cursor后移
chain_t *read_item(circular_chain_t *list) {
    if (list->current == NULL) {
        return NULL;
    }
    chain_t *item = list->current;
    list->current = item->next;
    return item;
}

chain_t *remove_item(circular_chain_t *list, chain_t *item) {
    if (list->size == 1) {
        if (item == list->current) {
            list->current = NULL;
            list->size = 0;
            return item;
        }
        return NULL;
    }
    for (int i = 0; i < list->size; ++i) {
        if (read_item(list) != item) {
            continue;
        }
        // 前面的元素
        item->previous->next = item->next;
        // 后面的元素
        item->next->previous = item->previous;
        // 当前item
        if (item == list->current) {
            list->current = item->previous;
        }

        list->size--;
        return item;
    }
    return NULL;// 未找到
}