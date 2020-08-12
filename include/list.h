#ifndef _LIST_H
#define _LIST_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct node {
	uint32_t x;
	struct node *next;
} node_t;

static inline node_t* last_node(node_t *node)
{
	while (node->next != NULL)
		node = node->next;
	return node;
}

static inline void insert_node(node_t *head, uint32_t data)
{
	node_t *node;

	node = malloc(sizeof(node_t));
	node->x = data;
	node->next = head->next;
	head->next = node;
}

static inline void append_node(node_t *head, uint32_t data)
{
	insert_node(last_node(head), data);
}

static inline void delete_node(node_t *node)
{
	node_t *tmp;

	if ((tmp = node->next) != NULL) {
		node->next = tmp->next;
		free(tmp);
	}
}

static inline node_t *init_node(void)
{
	node_t *new_node;

	new_node = malloc(sizeof(node_t));
	new_node->next = NULL;
	return new_node;
}

static inline void free_list(node_t *node)
{
	while (node->next != NULL)
		delete_node(node);
}

static inline node_t *search_node(node_t *node, uint32_t data)
{
	while (node->next != NULL) {
		node = node->next;
		if(data == node->x)
			return node;
	}
	return NULL;
}

static inline void print_node(node_t *node)
{
	while (node->next != NULL) {
		node = node->next;
		fprintf(stdout, "%u -> ", node->x);
	}
	fprintf(stdout, "NULL\n");
}

#endif /*_LIST_H */
