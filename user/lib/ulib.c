/* Mini "biblioteca" dos programas, escrita do zero: so o necessario. */
#include "processa.h"

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

unsigned long strlen(const char *s)
{
    unsigned long n = 0;
    while (s[n])
        n++;
    return n;
}

void *memset(void *dst, int c, unsigned long n)
{
    unsigned char *d = dst;
    while (n--)
        *d++ = (unsigned char)c;
    return dst;
}

void *memcpy(void *dst, const void *src, unsigned long n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void print(const char *s)
{
    sys_write(s, strlen(s));
}

#define OUT_MAX 256

struct out {
    char buf[OUT_MAX];
    int len;
};

static void put(struct out *o, char c)
{
    if (o->len < OUT_MAX)
        o->buf[o->len++] = c;
}

static void put_num(struct out *o, unsigned long v, int base, int neg, int width, char padc)
{
    char tmp[24];
    int n = 0;
    do {
        tmp[n++] = "0123456789abcdef"[v % base];
        v /= base;
    } while (v);
    for (int i = n + neg; i < width; i++)
        put(o, padc);
    if (neg)
        put(o, '-');
    while (n)
        put(o, tmp[--n]);
}

/* printf minimo: %d %u %x %p %s %c %%, largura e '0', modificador 'l'.
 * Monta a linha inteira e faz UMA chamada de sistema. */
void printf(const char *fmt, ...)
{
    struct out o;
    va_list ap;
    o.len = 0;
    va_start(ap, fmt);

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            put(&o, *fmt);
            continue;
        }
        fmt++;
        char padc = ' ';
        int width = 0, lng = 0;
        if (*fmt == '0') {
            padc = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');
        while (*fmt == 'l') {
            lng = 1;
            fmt++;
        }

        switch (*fmt) {
        case 'd': {
            long v = lng ? va_arg(ap, long) : va_arg(ap, int);
            put_num(&o, v < 0 ? -(unsigned long)v : (unsigned long)v, 10, v < 0, width, padc);
            break;
        }
        case 'u':
            put_num(&o, lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int), 10, 0, width, padc);
            break;
        case 'x':
            put_num(&o, lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int), 16, 0, width, padc);
            break;
        case 'p':
            put(&o, '0');
            put(&o, 'x');
            put_num(&o, (unsigned long)va_arg(ap, void *), 16, 0, width, padc);
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            while (*s)
                put(&o, *s++);
            break;
        }
        case 'c':
            put(&o, (char)va_arg(ap, int));
            break;
        case '%':
            put(&o, '%');
            break;
        case '\0':
            fmt--;
            break;
        default:
            put(&o, '%');
            put(&o, *fmt);
        }
    }
    va_end(ap);
    sys_write(o.buf, (unsigned long)o.len);
}
