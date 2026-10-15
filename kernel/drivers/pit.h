#ifndef DRIVERS_PIT_H
#define DRIVERS_PIT_H

#include "types.h"

#define PIT_MIN_HZ 19       /* divisor maximo do PIT e 65535 -> ~18,2 Hz */
#define PIT_MAX_HZ 1000

typedef void (*tick_callback_t)(void);

void pit_init(uint32_t hz, tick_callback_t on_tick);
void pit_set_frequency(uint32_t hz);
uint32_t pit_frequency(void);
uint32_t pit_divisor(void);
uint64_t pit_ticks(void);

#endif
