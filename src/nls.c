// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2020 LeavaTail
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "nls.h"

/**
 * utf8_to_utf32 - convert UTF-8 character to UTF-32
 * @u              UTF-8 character
 * @d              UTF-32 character (output)
 *
 * @return:        Byte size in UTF-32
 */
int utf8_to_utf32(unsigned char *u, uint32_t *d)
{
	int len = 0;
	unsigned char c = *u;

	/* 1 byte character   0x10?????? */
	if ((c & 0x80) == 0x00) {
		*d = c;
		len = 1;
	/* 2 bytes character  0x110????? 0x10?????? */
	} else if ((c & 0xE0) == 0xC0) {
		*d =  (*u & 0x1F) << 6;
		*d |= (*(u + 1) & 0x3F);
		len = 2;
	/* 3 bytes character  0x1110???? 0x10?????? 0x10??????*/
	} else if ((c & 0xF0) == 0xE0) {
		*d =  (*u & 0x0F) << 12;
		*d |= (*(u + 1) & 0x3F) << 6;
		*d |= (*(u + 2) & 0x3F);
		len = 3;
	/* 4 bytes character  0x11110??? 0x10?????? 0x10?????? 0x10??????*/
	} else if ((c & 0xF8) == 0xF0) {
		*d =  (*u & 0x07) << 18;
		*d |= (*(u + 1) & 0x3F) << 12;
		*d |= (*(u + 2) & 0x3F) << 6;
		*d |= (*(u + 3) & 0x3F);
		len = 4;
	} else {
		fprintf(stderr, "Unexpected sequences %02x.\n", c);
	}
	return len;
}
/**
 * utf32_to_utf8 - convert UTF-32 character to UTF-8
 * @u              UTF-32 character
 * @d              UTF-8 character (output)
 *
 * @return:        Byte size in UTF-8
 */
int utf32_to_utf8(uint32_t u, unsigned char *d)
{
	int len = 0;

	if (u < 0x7f) {
		*d = (unsigned char)u;
		len = 1;
	} else if (u < 0x7FF) {
		*d++ = (0xC0 | (u >> 6));
		*d++ = (0x80 | (u & 0x3F));
		len = 2;
	} else if (u < 0xFFFF) {
		*d++ = (0xE0 | (u >> 12));
		*d++ = (0x80 | ((u >> 6) & 0x3f));
		*d++ = (0x80 | (u & 0x3F));
		len = 3;
	} else if (u < 0x10FFFF) {
		*d++ = (0xF0 | (u >> 18));
		*d++ = (0x80 | ((u >> 12) & 0x3f));
		*d++ = (0x80 | ((u >> 6) & 0x3f));
		*d++ = (0x80 | (u & 0x3F));
		len = 4;
	} else {
		fprintf(stderr, "can't convert to U+%04x.\n", u);
	}
	return len;
}

/**
 * utf8s_to_utf16s - convert UTF-8 characters to UTF-16
 * @src              UTF-8 characters
 * @namelen          UTF-8 characters length
 * @dist             UTF-16 characters (output)
 *
 * @return:          byte size in UTF-16
 */
int utf8s_to_utf16s(unsigned char *src, uint16_t namelen, uint16_t* dist)
{
	int size = 0, len = 0, out_len = 0;
	unsigned char *u = src;
	uint32_t w;

	while (len < namelen) {
		u = src + len;
		w = (uint32_t)*u;
		size = utf8_to_utf32(u, &w);
		if (w <= 0xFFFF) {
			*(dist + out_len) = w;
			len += size;
			out_len ++;
		} else if (w <= UNICODE_MAX) {
			/* TODO: Implement surrogate pair */
			return 0;
		} else {
			fprintf(stderr, "Unicode doesn't support. (%0x)\n", w);
			return 0;
		}
	}
	return out_len;
}

/**
 * utf16s_to_utf8s - convert UTF-16 characters to UTF-8
 * @src              UTF-16 characters
 * @namelen          UTF-16 characters length
 * @dist             UTF-8 characters (output)
 *
 * @return:          byte size in UTF-8
 */
int utf16s_to_utf8s(uint16_t *src, uint16_t namelen, unsigned char* dist)
{
	int size, len = 0;
	uint16_t *u;
	uint32_t w;

	while (namelen--) {
		u = src++;
		w = (uint32_t)*u;
		if (*u < 0x7f) {
			/* 1 byte character */
			*(dist + len) = (unsigned char)*u;
			len++;
		} else {
			/* mult bytes character */
			switch (*u & SURROGATE_PAIR_MASK) {
				case SURROGATE_PAIR_UPPER:
					/* FALLTHROUGH */
				case SURROGATE_PAIR_LOWER:
					break;
				default:
					/* convert UTF32(w) to UTF8(dist) */
					size = utf32_to_utf8(w, dist + len);
					len += size;
					break;
			}
		}
	}
	return len;
}
