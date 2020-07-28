#ifndef _LIST2_H
#define _LIST2_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct node2 {
	uint64_t x;
	uint64_t y;
	struct node2 *next;
} node2_t;

static inline node2_t* last_node2(node2_t *node)
{
	while (node->next != NULL)
		node = node->next;
	return node;
}

static inline void insert_node2(node2_t *head, uint64_t x, uint64_t y)
{
	node2_t *node;

	node = (node2_t *)malloc(sizeof(node2_t));
	node->x = x;
	node->y = y;
	node->next = head->next;
	head->next = node;
}

static inline void append_node2(node2_t *head, uint64_t x, uint64_t y)
{
	insert_node2(last_node2(head), x, y);
}

static inline void delete_node2(node2_t *node)
{
	node2_t *tmp;

	if ((tmp = node->next) != NULL) {
		node->next = tmp->next;
		free(tmp);
	}
}

static inline node2_t *init_node2(uint64_t x, uint64_t y)
{
	node2_t *new_node;

	new_node = (node2_t *)malloc(sizeof(node2_t));
	new_node->x = x;
	new_node->y = y;
	new_node->next = NULL;
	return new_node;
}

static inline void free_list2(node2_t *node)
{
	while (node->next != NULL)
		delete_node2(node);
}

static inline node2_t *searchx_node2(node2_t *node, uint64_t x)
{
	while (node->next != NULL) {
		node = node->next;
		if(x == node->x)
			return node;
	}
	return NULL;
}

static inline node2_t *searchy_node2(node2_t *node, uint64_t y)
{
	while (node->next != NULL) {
		node = node->next;
		if(y == node->y)
			return node;
	}
	return NULL;
}

static inline void print_node2(node2_t *node)
{
	while (node->next != NULL) {
		node = node->next;
		fprintf(stdout, "(%lu, %lu) -> ", node->x, node->y);
	}
	fprintf(stdout, "NULL\n");
}

#endif /*_LIST2_H */
