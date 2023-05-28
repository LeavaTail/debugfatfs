// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2023 LeavaTail
 */
#ifndef _BITMAP_H
#define _BITMAP_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

typedef struct {
	uint8_t *data;
	size_t size;
} bitmap_t;

static inline int init_bitmap(bitmap_t *b, size_t s)
{
	b->data = calloc((s / CHAR_BIT) + 1, sizeof(uint8_t));
	b->size = s;

	return 0;
}

static inline int get_bitmap(bitmap_t *b, size_t value)
{
	size_t offset;
	size_t shift;
	uint8_t mask;

	offset = value / CHAR_BIT;
	shift = value % CHAR_BIT;
	mask = 1 << shift;

	return b->data[offset] & mask;
}

static inline int set_bitmap(bitmap_t *b, size_t value)
{
	size_t offset;
	size_t shift;
	uint8_t mask;

	offset = value / CHAR_BIT;
	shift = value % CHAR_BIT;
	mask = 1 << shift;

	b->data[offset] |= mask;

	return 0;
}

static inline int unset_bitmap(bitmap_t *b, size_t value)
{
	size_t offset;
	size_t shift;
	uint8_t mask;

	offset = value / CHAR_BIT;
	shift = value % CHAR_BIT;
	mask = 1 << shift;

	b->data[offset] &= ~mask;

	return 0;
}

static inline void free_bitmap(bitmap_t *b)
{
	free(b->data);
}

#endif /*_BITMAP_H */
