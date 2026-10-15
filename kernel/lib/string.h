#ifndef LIB_STRING_H
#define LIB_STRING_H

#include "types.h"

void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
void str_copy(char *dst, const char *src, size_t size);
long str_to_long(const char *s);   /* -1 se nao for numero */

#endif
