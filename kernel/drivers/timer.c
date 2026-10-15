#include "drivers/timer.h"
#include "drivers/lapic.h"

static bool lapic;

void timer_init(uint32_t hz, tick_callback_t on_tick, bool use_lapic, uint64_t tsc_hz)
{
    lapic = use_lapic && lapic_timer_init(hz, on_tick, tsc_hz);
    if (!lapic)
        pit_init(hz, on_tick);
}

void timer_set_frequency(uint32_t hz)
{
    if (lapic)
        lapic_timer_set_frequency(hz);
    else
        pit_set_frequency(hz);
}

uint32_t timer_frequency(void) { return lapic ? lapic_timer_frequency() : pit_frequency(); }
uint64_t timer_ticks(void)     { return lapic ? lapic_timer_ticks() : pit_ticks(); }
bool timer_is_lapic(void)      { return lapic; }
const char *timer_name(void)   { return lapic ? "APIC" : "PIT"; }
