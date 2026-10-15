#ifndef LIB_PRINTF_H
#define LIB_PRINTF_H

#include "types.h"
#include "lib/stdarg.h"

/* Suporta: %d %i %u %x %X %p %s %c %%, flags '-' '0' '#', largura,
 * precisao em %s (%.10s) e modificador 'l' (64 bits). */
int kvsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int ksnprintf(char *buf, size_t size, const char *fmt, ...);

void kprintf(const char *fmt, ...);                     /* log, cor padrao */
void kprintf_color(uint8_t color, const char *fmt, ...);
__attribute__((noreturn)) void panic(const char *fmt, ...);

#endif
