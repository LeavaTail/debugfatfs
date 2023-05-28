# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2023 LeavaTail

srcdir = ../../

CC := gcc
CFLAGS := -O0 --no-inline -I${srcdir}include/ -lcunit
LDFLAGS +=  -fprofile-generate

all: test

test: ${srcdir}src/nls.o test.c
	$(CC) $^ $(LDFLAGS) $(CFLAGS) -o $@

.PHONY: clean
clean:
	-rm -f test
	-rm -f *.gcno *.gcov *.gcda

.DEFAULT_GOAL=all
