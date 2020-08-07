#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "nls.h"

/**
 * utf32_to_utf8  - convert UTF-32 character to UTF-8
 * @u               UTF-32 character
 * @d               UTF-8 character (output)
 *
 * return:          Byte size in UTF-8
 * TODO: Take measures about Redundant UTF-8 Encoding
 */
int utf32_to_utf8(uint32_t u, unsigned char *d)
{
	int len = 0;

	if (u < 0x7f) {
		*d = (unsigned char)u;
		len = 1;
	} else if (u < 0x7FF) {
		*d++ = (0xC0 | u >> 6);
		*d++ = (0x80 | u & 0x3F);
		len = 2;
	} else if (u < 0xFFFF) {
		*d++ = (0xE0 | u >> 12);
		*d++ = (0x80 | (u >> 6) & 0x3f);
		*d++ = (0x80 | u & 0x3F);
		len = 3;
	} else if (u < 0x10FFFF) {
		*d++ = (0xF0 | u >> 18);
		*d++ = (0x80 | (u >> 12) & 0x3f);
		*d++ = (0x80 | (u >> 6) & 0x3f);
		*d++ = (0x80 | u & 0x3F);
		len = 4;
	} else {
		fprintf(stderr, "can't convert to U+%04x.\n", u);
	}
	return len;
}

/**
 * utf16s_to_utf8s  - convert UTF-16 characters to UTF-8
 * @src               UTF-16 characters
 * @namelen           UTF-16 characters length
 * @dist              UTF-8 characters (output)
 *
 * return:            byte size in UTF-8
 * TODO: Implement surrogate pair case
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
			switch (*u & SURROGATE_PAIR_MASK){
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
	return 0;
}
