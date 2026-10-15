/* Funcoes de memoria e texto escritas do zero (o kernel nao tem libc). */
#include "lib/string.h"

void *memset(void *dst, int c, size_t n)
{
    void *d = dst;
    __asm__ volatile ("rep stosb" : "+D"(d), "+c"(n) : "a"(c) : "memory");
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    void *d = dst;
    __asm__ volatile ("rep movsb" : "+D"(d), "+S"(src), "+c"(n) : : "memory");
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;
    if (d <= s || d >= s + n)
        return memcpy(dst, src, n);
    while (n--)
        d[n] = s[n];
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *x = a, *y = b;
    for (size_t i = 0; i < n; i++)
        if (x[i] != y[i])
            return x[i] - y[i];
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (uint8_t)*a - (uint8_t)*b;
}

void str_copy(char *dst, const char *src, size_t size)
{
    size_t i = 0;
    if (!size)
        return;
    for (; i + 1 < size && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

long str_to_long(const char *s)
{
    long v = 0;
    if (!s || !*s)
        return -1;
    for (; *s; s++) {
        if (*s < '0' || *s > '9')
            return -1;
        v = v * 10 + (*s - '0');
        if (v > 1000000000L)
            return -1;
    }
    return v;
}
