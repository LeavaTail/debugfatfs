#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "nls.h"

int utf16_to_utf8(uint16_t *src, uint16_t namelen, unsigned char* dist)
{
	uint16_t *u;
	uint32_t w;
	while (namelen--) {
		u = src++;
		w = (uint32_t)*u;
		if (*u < 0x7f) {
			/* 1 byte character */
			fprintf(stdout, "%c", (uint8_t)*u);
		} else {
			/* mult bytes character */
			switch (*u & SURROGATE_PAIR_MASK){
				case SURROGATE_PAIR_UPPER:
					/* FALLTHROUGH */
				case SURROGATE_PAIR_LOWER:
					break;
				default:
					/* convert UTF32(w) to UTF8(dist) */
					break;
			}
			fprintf(stdout, "%08x ", w);
		}
	}
	fprintf(stdout, "\n");
	return 0;
}
