#ifndef DRIVERS_LAPIC_H
#define DRIVERS_LAPIC_H

#include "types.h"
#include "drivers/pit.h"        /* tick_callback_t */

bool lapic_timer_init(uint32_t hz, tick_callback_t on_tick, uint64_t tsc_hz);
void lapic_timer_set_frequency(uint32_t hz);
void lapic_timer_irq(void);         /* chamado em isr_handler (vetor 0x40) */
void lapic_eoi(void);
bool lapic_active(void);
uint32_t lapic_timer_frequency(void);
uint64_t lapic_timer_ticks(void);
uint64_t lapic_timer_hz(void);      /* contagens do timer por segundo (calibrado) */
bool lapic_x2apic(void);

#endif
