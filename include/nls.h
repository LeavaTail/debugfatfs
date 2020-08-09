#ifndef _NLS_H
#define _NLS_H
#include <stdint.h>

#define SURROGATE_PAIR_MASK		0xF800	//1111 11?? ???? ????
#define SURROGATE_PAIR_UPPER	0xD800	//1101 10?? ???? ????
#define SURROGATE_PAIR_LOWER	0xDC00	//1101 11?? ???? ????

#define UNICODE_MAX     0x10FFFF

#define UTF8_MAX_CHARSIZE		4

int utf8s_to_utf16s(unsigned char *src, uint16_t namelen, uint16_t* dist);
int utf16s_to_utf8s(uint16_t *, uint16_t, unsigned char*);

#endif /*_NLS_H */
