#ifndef SMSBARE_CIRCLE_SMSBARE_FATFS_COMPAT_STRING_H
#define SMSBARE_CIRCLE_SMSBARE_FATFS_COMPAT_STRING_H

#include <stddef.h>

static inline void *memcpy(void *dest, const void *src, size_t n)
{
	return __builtin_memcpy(dest, src, n);
}

static inline void *memset(void *s, int c, size_t n)
{
	return __builtin_memset(s, c, n);
}

static inline int memcmp(const void *s1, const void *s2, size_t n)
{
	return __builtin_memcmp(s1, s2, n);
}

static inline char *strchr(const char *s, int c)
{
	return __builtin_strchr(s, c);
}

#endif
