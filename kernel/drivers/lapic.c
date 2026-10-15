/* =====================================================================
 *  DRIVER DO TIMER DO APIC LOCAL (LAPIC timer)
 * ---------------------------------------------------------------------
 *  Cada nucleo da CPU tem um "APIC local" com um timer proprio: ele conta
 *  para baixo a partir de um valor inicial e, ao chegar a zero, gera uma
 *  interrupcao; no modo periodico ele recarrega sozinho. Usado no boot
 *  UEFI, porque sem o modo Legacy/CSM muitos PCs desligam o chip PIT.
 *
 *  A velocidade desse contador nao e padronizada (depende da CPU), entao
 *  calibramos: contamos quanto ele anda em 10 ms medidos pelo TSC, cuja
 *  frequencia o carregador UEFI mediu com o Stall() do firmware.
 *
 *  O APIC e acessado por memoria (MMIO em 0xFEE00000) ou, se o firmware
 *  ligou o modo x2APIC, pelas instrucoes rdmsr/wrmsr.
 * ===================================================================== */
#include "drivers/lapic.h"
#include "arch/cpu.h"
#include "mm/vmm.h"

#define IA32_APIC_BASE     0x1B
#define APIC_BASE_X2APIC   (1UL << 10)
#define APIC_BASE_ENABLE   (1UL << 11)

#define REG_EOI         0x0B0
#define REG_SVR         0x0F0   /* liga o APIC + vetor espurio */
#define REG_LVT_TIMER   0x320
#define REG_LVT_THERMAL 0x330
#define REG_LVT_PERF    0x340
#define REG_LVT_ERROR   0x370
#define REG_TIMER_INIT  0x380   /* valor inicial do contador */
#define REG_TIMER_CUR   0x390   /* valor atual */
#define REG_TIMER_DIV   0x3E0

#define LVT_MASKED      (1 << 16)
#define LVT_PERIODIC    (1 << 17)

static volatile uint32_t *mmio;
static bool active, x2apic;
static uint64_t apic_hz;
static uint32_t frequency;
static volatile uint64_t ticks;
static tick_callback_t tick_callback;

static uint32_t rd(uint32_t reg)
{
    return x2apic ? (uint32_t)rdmsr(0x800 + (reg >> 4)) : mmio[reg / 4];
}

static void wr(uint32_t reg, uint32_t v)
{
    if (x2apic)
        wrmsr(0x800 + (reg >> 4), v);
    else
        mmio[reg / 4] = v;
}

bool lapic_timer_init(uint32_t hz, tick_callback_t on_tick, uint64_t tsc_hz)
{
    if (!tsc_hz)
        return false;

    uint64_t base = rdmsr(IA32_APIC_BASE);
    if (!(base & APIC_BASE_ENABLE)) {
        base |= APIC_BASE_ENABLE;
        wrmsr(IA32_APIC_BASE, base);
    }
    x2apic = base & APIC_BASE_X2APIC;
    if (!x2apic) {
        mmio = vmm_map_mmio(base & 0xFFFFFF000UL, 4096, true);
        if (!mmio)
            return false;
    }

    tick_callback = on_tick;
    wr(REG_SVR, 0x100 | LAPIC_SPURIOUS_VECTOR);     /* liga o APIC */
    wr(REG_LVT_ERROR, LVT_MASKED);                  /* nada de interrupcoes */
    wr(REG_LVT_THERMAL, LVT_MASKED);                /* que o kernel nao trata */
    wr(REG_LVT_PERF, LVT_MASKED);
    wr(REG_TIMER_DIV, 0x3);                         /* conta a cada 16 ciclos */

    /* calibracao: quanto o contador anda em 10 ms do TSC? */
    wr(REG_LVT_TIMER, LVT_MASKED);
    wr(REG_TIMER_INIT, 0xFFFFFFFF);
    uint64_t t0 = rdtsc(), wait = tsc_hz / 100;
    while (rdtsc() - t0 < wait)
        ;
    uint32_t elapsed = 0xFFFFFFFF - rd(REG_TIMER_CUR);
    wr(REG_TIMER_INIT, 0);
    apic_hz = (uint64_t)elapsed * 100;
    if (apic_hz < 1000)
        return false;

    active = true;
    lapic_timer_set_frequency(hz);
    return true;
}

void lapic_timer_set_frequency(uint32_t hz)
{
    if (hz < PIT_MIN_HZ)
        hz = PIT_MIN_HZ;
    if (hz > PIT_MAX_HZ)
        hz = PIT_MAX_HZ;
    frequency = hz;
    wr(REG_LVT_TIMER, LAPIC_TIMER_VECTOR | LVT_PERIODIC);
    wr(REG_TIMER_INIT, (uint32_t)(apic_hz / hz));   /* comeca a contar */
}

void lapic_eoi(void)
{
    if (active)
        wr(REG_EOI, 0);
}

void lapic_timer_irq(void)
{
    ticks++;
    if (tick_callback)
        tick_callback();
    lapic_eoi();
}

bool lapic_active(void)              { return active; }
bool lapic_x2apic(void)              { return x2apic; }
uint32_t lapic_timer_frequency(void) { return frequency; }
uint64_t lapic_timer_ticks(void)     { return ticks; }
uint64_t lapic_timer_hz(void)        { return apic_hz; }
