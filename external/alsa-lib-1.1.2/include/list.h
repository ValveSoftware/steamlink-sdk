/* Doubly linked list macros compatible with Linux kernel's version
 * Copyright (c) 2015 by Takashi Iwai <tiwai@suse.de>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 */

#ifndef _LIST_H
#define _LIST_H

#include <stddef.h>

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

/* one-shot definition of a list head */
#define LIST_HEAD(x) \
	struct list_head x = { &x, &x }

/* initialize a list head explicitly */
static inline void INIT_LIST_HEAD(struct list_head *p)
{
	p->next = p->prev = p;
}

#define list_entry_offset(p, type, offset) \
	((type *)((char *)(p) - (offset)))

/* list_entry - retrieve the original struct from list_head
 * @p: list_head pointer
 * @type: struct type
 * @member: struct field member containing the list_head
 */
#define list_entry(p, type, member) \
	list_entry_offset(p, type, offsetof(type, member))

/* list_for_each - iterate over the linked list
 * @p: iterator, a list_head pointer variable
 * @list: list_head pointer containing the list
 */
#define list_for_each(p, list) \
	for (p = (list)->next; p != (list); p = p->next)

/* list_for_each_safe - iterate over the linked list, safe to delete
 * @p: iterator, a list_head pointer variable
 * @s: a temporary variable to keep the next, a list_head pointer, too
 * @list: list_head pointer containing the list
 */
#define list_for_each_safe(p, s, list) \
	for (p = (list)->next; s = p->next, p != (list); p = s)

/* list_add - prepend a list entry at the head
 * @p: the new list entry to add
 * @list: the list head
 */
static inline void list_add(struct list_head *p, struct list_head *list)
{
	struct list_head *first = list->next;

	p->next = first;
	first->prev = p;
	list->next = p;
	p->prev = list;
}

/* list_add_tail - append a list entry at the tail
 * @p: the new list entry to add
 * @list: the list head
 */
static inline void list_add_tail(struct list_head *p, struct list_head *list)
{
	struct list_head *last = list->prev;

	last->next = p;
	p->prev = last;
	p->next = list;
	list->prev = p;
}

/* list_del - delete the given list entry */
static inline void list_del(struct list_head *p)
{
	p->prev->next = p->next;
	p->next->prev = p->prev;
}

/* list_empty - returns 1 if the given list is empty */
static inline int list_empty(const struct list_head *p)
{
	return p->next == p;
}

#endif /* _LIST_H */
