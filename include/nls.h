#ifndef _NLS_H
#define _NLS_H
#include <stdint.h>

#define SURROGATE_PAIR_MASK		0xF800	//1111 11?? ???? ????
#define SURROGATE_PAIR_UPPER	0xD800	//1101 10?? ???? ????
#define SURROGATE_PAIR_LOWER	0xDC00	//1101 11?? ???? ????

int utf16_to_utf8(uint16_t *, uint16_t, unsigned char*);

#endif /*_NLS_H */
