/*
 * libkmod - interface to kernel module operations
 *
 * Copyright (C) 2011-2013  ProFUSION embedded systems
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>

#include "libkmod.h"
#include "libkmod-internal.h"

/**
 * SECTION:libkmod-list
 * @short_description: general purpose list
 */

static inline struct list_node *list_node_init(struct list_node *node)
{
	node->next = node;
	node->prev = node;

	return node;
}

static inline struct list_node *list_node_next(const struct list_node *node)
{
	if (node == NULL)
		return NULL;

	return node->next;
}

static inline struct list_node *list_node_prev(const struct list_node *node)
{
	if (node == NULL)
		return NULL;

	return node->prev;
}

static inline void list_node_append(struct list_node *list,
							struct list_node *node)
{
	if (list == NULL) {
		list_node_init(node);
		return;
	}

	node->prev = list->prev;
	list->prev->next = node;
	list->prev = node;
	node->next = list;
}

static inline struct list_node *list_node_remove(struct list_node *node)
{
	if (node->prev == node || node->next == node)
		return NULL;

	node->prev->next = node->next;
	node->next->prev = node->prev;

	return node->next;
}

static inline void list_node_insert_after(struct list_node *list,
							struct list_node *node)
{
	if (list == NULL) {
		list_node_init(node);
		return;
	}

	node->prev = list;
	node->next = list->next;
	list->next->prev = node;
	list->next = node;
}

static inline void list_node_insert_before(struct list_node *list,
							struct list_node *node)
{
	if (list == NULL) {
		list_node_init(node);
		return;
	}

	node->next = list;
	node->prev = list->prev;
	list->prev->next = node;
	list->prev = node;
}

static inline void list_node_append_list(struct list_node *list1,
							struct list_node *list2)
{
	struct list_node *list1_last;

	if (list1 == NULL) {
		list_node_init(list2);
		return;
	}

	list1->prev->next = list2;
	list2->prev->next = list1;

	/* cache the last, because we will lose the pointer */
	list1_last = list1->prev;

	list1->prev = list2->prev;
	list2->prev = list1_last;
}

struct kmod_list *kmod_list_append(struct kmod_list *list, const void *data)
{
	struct kmod_list *new;

	new = malloc(sizeof(*new));
	if (new == NULL)
		return NULL;

	new->data = (void *)data;
	list_node_append(list ? &list->node : NULL, &new->node);

	return list ? list : new;
}

struct kmod_list *kmod_list_insert_after(struct kmod_list *list,
							const void *data)
{
	struct kmod_list *new;

	if (list == NULL)
		return kmod_list_append(list, data);

	new = malloc(sizeof(*new));
	if (new == NULL)
		return NULL;

	new->data = (void *)data;
	list_node_insert_after(&list->node, &new->node);

	return list;
}

struct kmod_list *kmod_list_insert_before(struct kmod_list *list,
							const void *data)
{
	struct kmod_list *new;

	if (list == NULL)
		return kmod_list_append(list, data);

	new = malloc(sizeof(*new));
	if (new == NULL)
		return NULL;

	new->data = (void *)data;
	list_node_insert_before(&list->node, &new->node);

	return new;
}

struct kmod_list *kmod_list_append_list(struct kmod_list *list1,
						struct kmod_list *list2)
{
	if (list1 == NULL)
		return list2;

	if (list2 == NULL)
		return list1;

	list_node_append_list(&list1->node, &list2->node);

	return list1;
}

struct kmod_list *kmod_list_prepend(struct kmod_list *list, const void *data)
{
	struct kmod_list *new;

	new = malloc(sizeof(*new));
	if (new == NULL)
		return NULL;

	new->data = (void *)data;
	list_node_append(list ? &list->node : NULL, &new->node);

	return new;
}

struct kmod_list *kmod_list_remove(struct kmod_list *list)
{
	struct list_node *node;

	if (list == NULL)
		return NULL;

	node = list_node_remove(&list->node);
	free(list);

	if (node == NULL)
		return NULL;

	return container_of(node, struct kmod_list, node);
}

struct kmod_list *kmod_list_remove_data(struct kmod_list *list,
							const void *data)
{
	struct kmod_list *itr;
	struct list_node *node;

	for (itr = list; itr != NULL; itr = kmod_list_next(list, itr)) {
		if (itr->data == data)
			break;
	}

	if (itr == NULL)
		return list;

	node = list_node_remove(&itr->node);
	free(itr);

	if (node == NULL)
		return NULL;

	return container_of(node, struct kmod_list, node);
}

/*
 * n must be greater to or equal the number of elements (we don't check the
 * condition)
 */
struct kmod_list *kmod_list_remove_n_latest(struct kmod_list *list,
							unsigned int n)
{
	struct kmod_list *l = list;
	unsigned int i;

	for (i = 0; i < n; i++) {
		l = kmod_list_last(l);
		l = kmod_list_remove(l);
	}

	return l;
}

/**
 * kmod_list_prev:
 * @list: the head of the list
 * @curr: the current node in the list
 *
 * Get the previous node in @list relative to @curr as if @list was not a
 * circular list. I.e.: the previous of the head is NULL. It can be used to
 * iterate a list by checking for NULL return to know when all elements were
 * iterated.
 *
 * Returns: node previous to @curr or NULL if either this node is the head of
 * the list or the list is empty.
 */
KMOD_EXPORT struct kmod_list *kmod_list_prev(const struct kmod_list *list,
						const struct kmod_list *curr)
{
	if (list == NULL || curr == NULL)
		return NULL;

	if (list == curr)
		return NULL;

	return container_of(curr->node.prev, struct kmod_list, node);
}

/**
 * kmod_list_next:
 * @list: the head of the list
 * @curr: the current node in the list
 *
 * Get the next node in @list relative to @curr as if @list was not a circular
 * list. I.e. calling this function in the last node of the list returns
 * NULL.. It can be used to iterate a list by checking for NULL return to know
 * when all elements were iterated.
 *
 * Returns: node next to @curr or NULL if either this node is the last of or
 * list is empty.
 */
KMOD_EXPORT struct kmod_list *kmod_list_next(const struct kmod_list *list,
						const struct kmod_list *curr)
{
	if (list == NULL || curr == NULL)
		return NULL;

	if (curr->node.next == &list->node)
		return NULL;

	return container_of(curr->node.next, struct kmod_list, node);
}

/**
 * kmod_list_last:
 * @list: the head of the list
 *
 * Get the last element of the @list. As @list is a circular list,
 * this is a cheap operation O(1) with the last element being the
 * previous element.
 *
 * If the list has a single element it will return the list itself (as
 * expected, and this is what differentiates from kmod_list_prev()).
 *
 * Returns: last node at @list or NULL if the list is empty.
 */
KMOD_EXPORT struct kmod_list *kmod_list_last(const struct kmod_list *list)
{
	if (list == NULL)
		return NULL;
	return container_of(list->node.prev, struct kmod_list, node);
}
