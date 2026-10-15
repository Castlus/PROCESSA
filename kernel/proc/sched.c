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
#include "drivers/console.h"
#include "drivers/timer.h"
#include "lib/printf.h"

static struct process *current;
static bool need_resched;
static const char *resched_reason = "";
static uint32_t quantum = DEFAULT_QUANTUM;
static uint32_t slice_left;
static uint64_t uptime_us;
static uint64_t total_switches;
static bool trace;

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
    uint64_t dt = 1000000UL / timer_frequency();
    uptime_us += dt;
    current->cpu_us += dt;

    /* acorda quem terminou de dormir */
    for (int i = 1; i < MAX_PROCS; i++) {
        struct process *p = proc_slot(i);
        if (p->state == P_SLEEPING && uptime_us >= p->wake_us)
            p->state = P_READY;
    }

    if (current->pid == 0) {
        if (any_ready()) {
            need_resched = true;        /* CPU ociosa e alguem quer rodar */
            resched_reason = "CPU estava ociosa";
        }
    } else if (slice_left <= 1) {
        need_resched = true;            /* acabou a fatia de tempo: PREEMPCAO */
        resched_reason = "quantum esgotado";
    } else {
        slice_left--;
    }
}

/* Por que o processo anterior saiu da CPU (aparece no trace). */
static const char *switch_reason(const struct process *prev)
{
    if (prev->state == P_DEAD) {
        if (prev->exit_code == -9)
            return "encerrado por kill";
        if (prev->exit_code == -1)
            return "encerrado por exceção";
        return "terminou com exit";
    }
    if (prev->state == P_SLEEPING)
        return "dormiu com sleep";
    return resched_reason;
}

static void trace_switch(const struct process *prev, const struct process *next, const char *why)
{
    kprintf_color(COLOR_TRACE, "[troca %lu | tick %lu] %d:%.14s -> %d:%.14s (%s)\n",
                  total_switches, timer_ticks(), prev->pid, prev->name, next->pid, next->name, why);
    if (prev->state == P_DEAD)
        kprintf_color(COLOR_TRACE, "   %d liberado | %d retoma em rip=%lx, CR3=%08lx\n",
                      prev->pid, next->pid, next->ctx->rip, next->cr3);
    else
        kprintf_color(COLOR_TRACE, "   %d pausado em rip=%lx | %d retoma em rip=%lx, CR3=%08lx\n",
                      prev->pid, prev->ctx->rip, next->pid, next->ctx->rip, next->cr3);
}

static void schedule(void)
{
    struct process *prev = current;
    struct process *next = NULL;
    const char *why = switch_reason(prev);

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
        if (trace)
            trace_switch(prev, next, why);
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
    resched_reason = "cedeu a CPU com yield";
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

/* Ordem em que o round-robin vai escolher os processos PRONTOS. */
int sched_ready_queue(int *pids, int max)
{
    int n = 0, start = proc_index(current);
    for (int i = 1; i <= MAX_PROCS && n < max; i++) {
        struct process *p = proc_slot((start + i) % MAX_PROCS);
        if (p->pid != 0 && p->state == P_READY)
            pids[n++] = p->pid;
    }
    return n;
}

void sched_set_trace(bool on)     { trace = on; }
bool sched_trace(void)            { return trace; }
uint32_t sched_quantum(void)      { return quantum; }
uint32_t sched_slice_left(void)   { return current->pid == 0 ? 0 : slice_left; }
uint64_t sched_switches(void)     { return total_switches; }
uint64_t sched_uptime_ms(void)    { return uptime_us / 1000; }
