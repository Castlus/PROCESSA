#ifndef ARCH_IO_H
#define ARCH_IO_H

#include "types.h"

/* Portas de E/S: a CPU x86 conversa com os dispositivos (timer, teclado,
 * disco, controlador de interrupcoes, porta serial) pelas instrucoes IN/OUT. */

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

/* Le "count" palavras de 16 bits da porta para a memoria (usado pelo disco). */
static inline void insw(uint16_t port, void *dst, uint64_t count)
{
    __asm__ volatile ("rep insw" : "+D"(dst), "+c"(count) : "d"(port) : "memory");
}

/* Pequena pausa: escrever na porta 0x80 (sem uso) leva ~1 microssegundo. */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

#endif
