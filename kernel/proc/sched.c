/* =====================================================================
 *  ESCALONADOR ROUND-ROBIN PREEMPTIVO
 * ---------------------------------------------------------------------
 *  Cada processo recebe uma fatia de tempo (quantum) medida em ticks do
 *  timer. A cada tick, sched_tick() desconta da fatia; quando ela acaba,
 *  marca need_resched. Ao final da interrupcao, sched_switch() escolhe o
 *  proximo processo PRONTO da tabela, em fila circular, e devolve o
 *  contexto dele para isr.asm, que troca a pilha e faz iretq.
 *
 *  Linha do tempo de uma troca (processo A -> processo B):
 *    1. A esta rodando em modo usuario
 *    2. o PIT dispara a IRQ0 -> CPU salta para isr_stub_32
 *    3. isr_common empilha os registradores de A na pilha de kernel de A
 *    4. isr_handler: A->ctx = registradores; sched_tick(): fatia acabou
 *    5. schedule(): A vira PRONTO, B vira EXECUTANDO,
 *       TSS.rsp0 = pilha de kernel de B, CR3 = tabela de paginas de B
 *    6. isr_common: RSP = B->ctx, desempilha registradores de B, iretq
 *    7. B continua exatamente de onde tinha parado
 * ===================================================================== */
#include "proc/sched.h"
#include "arch/gdt.h"
#include "drivers/pit.h"

static struct process *current;
static bool need_resched;
static uint32_t quantum = DEFAULT_QUANTUM;
static uint32_t slice_left;
static uint64_t uptime_us;
static uint64_t total_switches;

void sched_init(void)
{
    current = proc_init_idle();
    slice_left = quantum;
}

struct process *sched_current(void)
{
    return current;
}

void sched_save_context(struct regs *r)
{
    current->ctx = r;
}

static bool any_ready(void)
{
    for (int i = 1; i < MAX_PROCS; i++)
        if (proc_slot(i)->state == P_READY)
            return true;
    return false;
}

void sched_tick(void)
{
    uint64_t dt = 1000000UL / pit_frequency();
    uptime_us += dt;
    current->cpu_us += dt;

    /* acorda quem terminou de dormir */
    for (int i = 1; i < MAX_PROCS; i++) {
        struct process *p = proc_slot(i);
        if (p->state == P_SLEEPING && uptime_us >= p->wake_us)
            p->state = P_READY;
    }

    if (current->pid == 0) {
        if (any_ready())
            need_resched = true;        /* CPU ociosa e alguem quer rodar */
    } else if (slice_left <= 1) {
        need_resched = true;            /* acabou a fatia de tempo: PREEMPCAO */
    } else {
        slice_left--;
    }
}

static void schedule(void)
{
    struct process *prev = current;
    struct process *next = NULL;

    need_resched = false;
    proc_reap(prev);
    if (prev->state == P_RUNNING)
        prev->state = P_READY;

    /* round-robin: procura o proximo PRONTO depois do atual, em circulo */
    int start = proc_index(prev);
    for (int i = 1; i <= MAX_PROCS; i++) {
        struct process *p = proc_slot((start + i) % MAX_PROCS);
        if (p->pid != 0 && p->state == P_READY) {
            next = p;
            break;
        }
    }
    if (!next)
        next = proc_slot(0);            /* ninguem pronto: CPU fica ociosa */

    next->state = P_RUNNING;
    slice_left = quantum;
    if (next != prev) {
        total_switches++;
        next->switches++;
    }
    current = next;

    if (next->kstack_top)
        tss_set_kernel_stack(next->kstack_top);
    if (read_cr3() != next->cr3)
        write_cr3(next->cr3);           /* troca o espaco de enderecamento */
}

uint64_t sched_switch(void)
{
    if (need_resched || current->state != P_RUNNING)
        schedule();
    return (uint64_t)current->ctx;
}

void sched_yield(void)
{
    need_resched = true;
}

void sched_sleep_ms(uint64_t ms)
{
    current->state = P_SLEEPING;
    current->wake_us = uptime_us + ms * 1000;
    need_resched = true;
}

void sched_set_quantum(uint32_t ticks)
{
    quantum = ticks ? ticks : 1;
}

uint32_t sched_quantum(void)   { return quantum; }
uint64_t sched_switches(void)  { return total_switches; }
uint64_t sched_uptime_ms(void) { return uptime_us / 1000; }
