#ifndef DRIVERS_TIMER_H
#define DRIVERS_TIMER_H

#include "types.h"
#include "drivers/pit.h"

/* Interface unica do "driver de timer". Por baixo ha dois dispositivos:
 *   PIT 8254         - boot por BIOS (QEMU, PCs com Legacy/CSM)
 *   timer do APIC    - boot por UEFI (o PIT costuma estar desligado)
 * O escalonador, o painel e o shell so usam estas funcoes. */
#define TIMER_MIN_HZ PIT_MIN_HZ
#define TIMER_MAX_HZ PIT_MAX_HZ

void timer_init(uint32_t hz, tick_callback_t on_tick, bool use_lapic, uint64_t tsc_hz);
void timer_set_frequency(uint32_t hz);
uint32_t timer_frequency(void);
uint64_t timer_ticks(void);
bool timer_is_lapic(void);
const char *timer_name(void);

#endif
