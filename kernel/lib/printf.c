/* printf do kernel, escrito do zero. */
#include "lib/printf.h"
#include "arch/cpu.h"
#include "drivers/console.h"

struct out {
    char *buf;
    size_t size;
    size_t len;
};

static void put(struct out *o, char c)
{
    if (o->len + 1 < o->size)
        o->buf[o->len] = c;
    o->len++;
}

static void pad(struct out *o, char c, int n)
{
    while (n-- > 0)
        put(o, c);
}

static void put_number(struct out *o, uint64_t v, unsigned base, bool neg,
                       int width, bool left, bool zero, bool upper, bool prefix)
{
    const char *set = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char digits[24];
    int n = 0;

    do {
        digits[n++] = set[v % base];
        v /= base;
    } while (v);

    int extra = (neg ? 1 : 0) + (prefix ? 2 : 0);
    int padding = width - n - extra;

    if (!left && !zero)
        pad(o, ' ', padding);
    if (neg)
        put(o, '-');
    if (prefix) {
        put(o, '0');
        put(o, 'x');
    }
    if (!left && zero)
        pad(o, '0', padding);
    while (n)
        put(o, digits[--n]);
    if (left)
        pad(o, ' ', padding);
}

int kvsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    struct out o = { buf, size, 0 };

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            put(&o, *fmt);
            continue;
        }
        fmt++;

        bool left = false, zero = false, alt = false;
        for (;; fmt++) {
            if (*fmt == '-')
                left = true;
            else if (*fmt == '0')
                zero = true;
            else if (*fmt == '#')
                alt = true;
            else
                break;
        }

        int width = 0;
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');

        int prec = -1;
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            while (*fmt >= '0' && *fmt <= '9')
                prec = prec * 10 + (*fmt++ - '0');
        }

        int lng = 0;
        while (*fmt == 'l') {
            lng++;
            fmt++;
        }

        switch (*fmt) {
        case 'd':
        case 'i': {
            int64_t v = lng ? va_arg(ap, int64_t) : va_arg(ap, int);
            uint64_t u = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
            put_number(&o, u, 10, v < 0, width, left, zero, false, false);
            break;
        }
        case 'u': {
            uint64_t v = lng ? va_arg(ap, uint64_t) : va_arg(ap, unsigned int);
            put_number(&o, v, 10, false, width, left, zero, false, false);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t v = lng ? va_arg(ap, uint64_t) : va_arg(ap, unsigned int);
            put_number(&o, v, 16, false, width, left, zero, *fmt == 'X', alt);
            break;
        }
        case 'p':
            put_number(&o, (uint64_t)va_arg(ap, void *), 16, false, width, left, zero, false, true);
            break;
        case 'c': {
            char c = (char)va_arg(ap, int);
            if (!left)
                pad(&o, ' ', width - 1);
            put(&o, c);
            if (left)
                pad(&o, ' ', width - 1);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s)
                s = "(null)";
            int len = 0;
            while (s[len] && (prec < 0 || len < prec))
                len++;
            if (!left)
                pad(&o, ' ', width - len);
            for (int i = 0; i < len; i++)
                put(&o, s[i]);
            if (left)
                pad(&o, ' ', width - len);
            break;
        }
        case '%':
            put(&o, '%');
            break;
        case '\0':
            fmt--;
            break;
        default:
            put(&o, '%');
            put(&o, *fmt);
            break;
        }
    }

    if (size)
        o.buf[o.len < size ? o.len : size - 1] = '\0';
    return (int)o.len;
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = kvsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

/* O kernel so roda com interrupcoes desligadas (dentro dos tratadores),
 * entao um buffer estatico e seguro: nunca ha duas chamadas ao mesmo tempo. */
static char line[512];

static void kvprintf_color(uint8_t color, const char *fmt, va_list ap)
{
    int n = kvsnprintf(line, sizeof line, fmt, ap);
    if (n > (int)sizeof line - 1)
        n = sizeof line - 1;
    console_write(line, (size_t)n, color);
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf_color(COLOR_KERNEL, fmt, ap);
    va_end(ap);
}

void kprintf_color(uint8_t color, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf_color(color, fmt, ap);
    va_end(ap);
}

void panic(const char *fmt, ...)
{
    va_list ap;
    __asm__ volatile ("cli");
    console_write("KERNEL PANIC: ", 14, COLOR_ERROR);
    va_start(ap, fmt);
    kvprintf_color(COLOR_ERROR, fmt, ap);
    va_end(ap);
    console_write("\n", 1, COLOR_ERROR);
    cpu_halt_forever();
}
