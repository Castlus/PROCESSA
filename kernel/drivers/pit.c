/* =====================================================================
 *  DRIVER DO TIMER: PIT 8253/8254 (Programmable Interval Timer)
 * ---------------------------------------------------------------------
 *  O PIT e um chip com um oscilador de 1.193.182 Hz. Programamos o
 *  canal 0 com um DIVISOR: a cada "divisor" pulsos ele gera a IRQ0.
 *      frequencia = 1193182 / divisor
 *  Ex.: divisor 11931 -> 100 interrupcoes por segundo (uma a cada 10 ms).
 *
 *  E essa interrupcao periodica que permite ao kernel retomar o controle
 *  da CPU mesmo que um processo nunca devolva a CPU por conta propria
 *  (multitarefa PREEMPTIVA).
 *
 *  O driver nao sabe nada de processos: ele so conta ticks e chama a
 *  funcao registrada (on_tick). Quem decide trocar de processo e o
 *  escalonador (proc/sched.c).
 * ===================================================================== */
#include "drivers/pit.h"
#include "arch/io.h"
#include "arch/irq.h"

#define PIT_BASE_HZ  1193182
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

static volatile uint64_t ticks;
static uint32_t frequency;
static uint32_t divisor;
static tick_callback_t tick_callback;

static void pit_irq(struct regs *r)
{
    (void)r;
    ticks++;
    if (tick_callback)
        tick_callback();
}

void pit_set_frequency(uint32_t hz)
{
    if (hz < PIT_MIN_HZ)
        hz = PIT_MIN_HZ;
    if (hz > PIT_MAX_HZ)
        hz = PIT_MAX_HZ;

    divisor = PIT_BASE_HZ / hz;
    frequency = hz;

    /* comando 0x36 = canal 0 | envia byte baixo e depois alto |
     *                modo 3 (onda quadrada) | contagem binaria */
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

void pit_init(uint32_t hz, tick_callback_t on_tick)
{
    tick_callback = on_tick;
    pit_set_frequency(hz);
    irq_register(0, pit_irq);           /* IRQ0 = timer */
}

uint32_t pit_frequency(void) { return frequency; }
uint32_t pit_divisor(void)   { return divisor; }
uint64_t pit_ticks(void)     { return ticks; }
