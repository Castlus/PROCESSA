/* Despachante central: isr.asm chama isr_handler() em TODA interrupcao. */
#include "arch/cpu.h"
#include "arch/irq.h"
#include "drivers/console.h"
#include "drivers/lapic.h"
#include "lib/printf.h"
#include "proc/process.h"
#include "proc/sched.h"
#include "proc/syscall.h"

static const char *const exception_names[32] = {
    "divisao por zero", "debug", "NMI", "breakpoint", "overflow",
    "BOUND fora do limite", "opcode invalido", "FPU indisponivel",
    "double fault", "coprocessor segment overrun", "TSS invalida",
    "segmento ausente", "falha de pilha", "protecao geral (GPF)",
    "page fault", "reservada", "erro de FPU x87", "alinhamento",
    "machine check", "erro SIMD", "virtualizacao", "control protection",
    "reservada", "reservada", "reservada", "reservada", "reservada",
    "reservada", "hypervisor injection", "VMM communication",
    "seguranca", "reservada",
};

static void kernel_fault(struct regs *r)
{
    kprintf_color(COLOR_ERROR, "\n*** EXCECAO NO KERNEL: %s (vetor %lu, erro 0x%lx)\n",
                  exception_names[r->vector], r->vector, r->error);
    kprintf_color(COLOR_ERROR, "rip=%016lx rsp=%016lx rflags=%08lx cr2=%016lx\n",
                  r->rip, r->rsp, r->rflags, read_cr2());
    kprintf_color(COLOR_ERROR, "rax=%016lx rbx=%016lx rcx=%016lx rdx=%016lx\n",
                  r->rax, r->rbx, r->rcx, r->rdx);
    panic("sistema parado");
}

static void handle_exception(struct regs *r)
{
    struct process *p = sched_current();

    if ((r->cs & 3) != 3)               /* erro dentro do proprio kernel */
        kernel_fault(r);

    /* Erro de um processo de usuario: so ELE e encerrado. O kernel e os
     * outros processos continuam rodando normalmente. */
    if (r->vector == 14)
        kprintf_color(COLOR_ERROR,
                      "[kernel] PID %d (%s): page fault no endereco %p (rip=%p). Processo encerrado.\n",
                      p->pid, p->name, (void *)read_cr2(), (void *)r->rip);
    else
        kprintf_color(COLOR_ERROR,
                      "[kernel] PID %d (%s): excecao '%s' (rip=%p). Processo encerrado.\n",
                      p->pid, p->name, exception_names[r->vector], (void *)r->rip);
    proc_kill(p, -1);
}

uint64_t isr_handler(struct regs *r)
{
    /* 1. guarda onde ficou o contexto do processo interrompido */
    sched_save_context(r);

    /* 2. trata o evento */
    if (r->vector < 32)
        handle_exception(r);
    else if (r->vector < 48)
        irq_dispatch((int)(r->vector - 32), r);
    else if (r->vector == SYSCALL_VECTOR)
        syscall_dispatch(r);
    else if (r->vector == LAPIC_TIMER_VECTOR)
        lapic_timer_irq();
    else if (r->vector == OTHER_VECTOR)
        lapic_eoi();            /* inesperada: so confirma, para nao travar o APIC */
    /* LAPIC_SPURIOUS_VECTOR: interrupcao "fantasma", nao precisa de nada */

    /* 3. devolve o contexto a retomar: o mesmo processo ou outro,
     *    se o escalonador decidiu trocar. */
    return sched_switch();
}
