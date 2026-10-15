#ifndef ARCH_IRQ_H
#define ARCH_IRQ_H

#include "arch/cpu.h"

typedef void (*irq_handler_t)(struct regs *r);

/* Um driver registra sua rotina para uma linha de IRQ (0-15). */
void irq_register(int irq, irq_handler_t handler);
void irq_dispatch(int irq, struct regs *r);

#endif
