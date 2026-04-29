#ifndef SMSBARE_CIRCLE_SMSBARE_STRING_H
#define SMSBARE_CIRCLE_SMSBARE_STRING_H

#include <stddef.h>

static inline void *memcpy(void *dest, const void *src, size_t n)
{
	return __builtin_memcpy(dest, src, n);
}

static inline void *memmove(void *dest, const void *src, size_t n)
{
	return __builtin_memmove(dest, src, n);
}

static inline void *memset(void *s, int c, size_t n)
{
	return __builtin_memset(s, c, n);
}

static inline int memcmp(const void *s1, const void *s2, size_t n)
{
	return __builtin_memcmp(s1, s2, n);
}

static inline size_t strlen(const char *s)
{
	return __builtin_strlen(s);
}

static inline int strcmp(const char *s1, const char *s2)
{
	return __builtin_strcmp(s1, s2);
}

static inline int strncmp(const char *s1, const char *s2, size_t n)
{
	return __builtin_strncmp(s1, s2, n);
}

static inline char *strcpy(char *dest, const char *src)
{
	return __builtin_strcpy(dest, src);
}

static inline char *strncpy(char *dest, const char *src, size_t n)
{
	return __builtin_strncpy(dest, src, n);
}

static inline char *strchr(const char *s, int c)
{
	return __builtin_strchr(s, c);
}

static inline char *strrchr(const char *s, int c)
{
	return __builtin_strrchr(s, c);
}

static inline char *strstr(const char *haystack, const char *needle)
{
	return __builtin_strstr(haystack, needle);
}

#endif
