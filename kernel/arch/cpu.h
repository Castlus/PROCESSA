#ifndef ARCH_CPU_H
#define ARCH_CPU_H

#include "types.h"

/* Registradores salvos na pilha de kernel a cada interrupcao.
 * A ordem e exatamente a inversa dos "push" de isr.asm, seguida do que a
 * propria CPU empilha ao receber a interrupcao (rip, cs, rflags, rsp, ss).
 * Isto E o "contexto" de um processo: com ele o processo pode ser pausado
 * e retomado depois exatamente de onde parou. */
struct regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector, error;
    uint64_t rip, cs, rflags, rsp, ss;
};

/* Seletores de segmento (indices na GDT, ver gdt.c). "| 3" = anel 3. */
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_DS   (0x18 | 3)
#define USER_CS   (0x20 | 3)
#define TSS_SEL   0x28

#define SYSCALL_VECTOR        0x80
#define LAPIC_TIMER_VECTOR    0x40  /* timer do APIC (boot UEFI)          */
#define OTHER_VECTOR          0xFE  /* qualquer interrupcao nao esperada   */
#define LAPIC_SPURIOUS_VECTOR 0xFF  /* interrupcao "fantasma" do APIC      */
#define RFLAGS_IF      0x200   /* bit "interrupcoes habilitadas" */

static inline uint64_t read_cr2(void)
{
    uint64_t v;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(v));
    return v;
}

static inline uint64_t read_cr3(void)
{
    uint64_t v;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(v));
    return v;
}

/* Trocar o CR3 troca o espaco de enderecamento inteiro (tabela de paginas). */
static inline void write_cr3(uint64_t v)
{
    __asm__ volatile ("mov %0, %%cr3" : : "r"(v) : "memory");
}

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t v)
{
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"((uint32_t)v), "d"((uint32_t)(v >> 32)));
}

/* Contador de ciclos da CPU (Time Stamp Counter). */
static inline uint64_t rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline __attribute__((noreturn)) void cpu_halt_forever(void)
{
    for (;;)
        __asm__ volatile ("cli; hlt");
}

#endif
