#include "arch/irq.h"
#include "drivers/pic.h"

static irq_handler_t handlers[16];

void irq_register(int irq, irq_handler_t handler)
{
    handlers[irq] = handler;
    pic_unmask(irq);
}

void irq_dispatch(int irq, struct regs *r)
{
    if (pic_is_spurious(irq))
        return;
    if (handlers[irq])
        handlers[irq](r);
    pic_eoi(irq);   /* avisa o PIC que a IRQ foi tratada */
}
