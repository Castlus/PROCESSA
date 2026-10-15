#ifndef DRIVERS_PIC_H
#define DRIVERS_PIC_H

#include "types.h"

void pic_init(void);
void pic_unmask(int irq);
void pic_eoi(int irq);
bool pic_is_spurious(int irq);

#endif
