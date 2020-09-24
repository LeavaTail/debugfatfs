// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2020 LeavaTail
 */
#ifndef _LIST2_H
#define _LIST2_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct node2 {
	uint32_t index;
	void *data;
	struct node2 *next;
} node2_t;

static inline node2_t *last_node2(node2_t *node)
{
	while (node->next != NULL)
		node = node->next;
	return node;
}

static inline void insert_node2(node2_t *head, uint32_t i, void *d)
{
	node2_t *node;

	node = malloc(sizeof(node2_t));
	node->index = i;
	node->data = d;
	node->next = head->next;
	head->next = node;
}

static inline void append_node2(node2_t *head, uint32_t i, void *d)
{
	insert_node2(last_node2(head), i, d);
}

static inline void delete_node2(node2_t *node)
{
	node2_t *tmp;

	if ((tmp = node->next) != NULL) {
		node->next = tmp->next;
		free(tmp->data);
		free(tmp);
	}
}

static inline node2_t *init_node2(uint32_t i, void *d)
{
	node2_t *new_node;

	new_node = malloc(sizeof(node2_t));
	new_node->index = i;
	new_node->data = d;
	new_node->next = NULL;
	return new_node;
}

static inline void free_list2(node2_t *node)
{
	if (!node)
		return;

	while (node->next != NULL)
		delete_node2(node);
}

static inline node2_t *search_node2(node2_t *node, uint32_t i)
{
	while (node->next != NULL) {
		node = node->next;
		if (i == node->index)
			return node;
	}
	return NULL;
}

static inline void print_node2(node2_t *node)
{
	while (node->next != NULL) {
		node = node->next;
		fprintf(stdout, "%u: (%p) -> ", node->index, node->data);
	}
	fprintf(stdout, "NULL\n");
}

#endif /*_LIST2_H */
