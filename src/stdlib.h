#ifndef SMSBARE_CIRCLE_SMSBARE_STDLIB_H
#define SMSBARE_CIRCLE_SMSBARE_STDLIB_H

#include <stddef.h>
#include <circle/alloc.h>

static inline int abs(int value)
{
	return value < 0 ? -value : value;
}

#endif
