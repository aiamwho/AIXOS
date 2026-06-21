#ifndef AIXOS_LIST_H
#define AIXOS_LIST_H

#include <stddef.h>

/*
 * 通用双向链表 (内核中使用)
 * 参考 Linux kernel list.h 设计
 */

typedef struct aixos_list {
    struct aixos_list *next;
    struct aixos_list *prev;
} aixos_list_t;

#define AIXOS_LIST_INIT(name)  { &(name), &(name) }

static inline void aixos_list_init(aixos_list_t *list)
{
    list->next = list;
    list->prev = list;
}

static inline void __aixos_list_add(aixos_list_t *new,
                                    aixos_list_t *prev,
                                    aixos_list_t *next)
{
    next->prev = new;
    new->next  = next;
    new->prev  = prev;
    prev->next = new;
}

static inline void aixos_list_add(aixos_list_t *new, aixos_list_t *head)
{
    __aixos_list_add(new, head, head->next);
}

static inline void aixos_list_add_tail(aixos_list_t *new, aixos_list_t *head)
{
    __aixos_list_add(new, head->prev, head);
}

static inline void __aixos_list_del(aixos_list_t *prev, aixos_list_t *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void aixos_list_del(aixos_list_t *entry)
{
    __aixos_list_del(entry->prev, entry->next);
    entry->next = (aixos_list_t *)0;
    entry->prev = (aixos_list_t *)0;
}

static inline int aixos_list_is_empty(const aixos_list_t *head)
{
    return head->next == head;
}

/* 从链表 head 中取出第一个节点, 不移除 */
static inline aixos_list_t *aixos_list_first(const aixos_list_t *head)
{
    return head->next;
}

#define AIXOS_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define AIXOS_LIST_FOR_EACH(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define AIXOS_LIST_FOR_EACH_SAFE(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

#endif /* AIXOS_LIST_H */
