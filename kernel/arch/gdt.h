#ifndef ARCH_GDT_H
#define ARCH_GDT_H

#include "types.h"

void gdt_init(void);
void tss_set_kernel_stack(uint64_t rsp0);

#endif
