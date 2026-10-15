/* GDT (Global Descriptor Table) e TSS.
 *
 * Em 64 bits a segmentacao quase nao existe, mas a GDT ainda define o
 * NIVEL DE PRIVILEGIO: anel 0 (kernel) e anel 3 (processos de usuario).
 * Um processo em anel 3 nao pode executar instrucoes privilegiadas
 * (cli, hlt, out, mov cr3...) nem acessar paginas do kernel. */
#include "arch/gdt.h"
#include "arch/cpu.h"
#include "lib/string.h"

/* TSS (Task State Segment): em 64 bits serve para dizer a CPU qual pilha
 * de kernel usar (rsp0) quando uma interrupcao chega enquanto um processo
 * esta em modo usuario. O escalonador atualiza rsp0 a cada troca. */
struct tss {
    uint32_t reserved0;
    uint64_t rsp0, rsp1, rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

struct gdtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static uint64_t gdt[7] __attribute__((aligned(16)));
static struct tss tss __attribute__((aligned(16)));

void gdt_init(void)
{
    gdt[0] = 0;                         /* descritor nulo (obrigatorio)        */
    gdt[1] = 0x00AF9A000000FFFFUL;      /* 0x08 codigo do kernel, anel 0, 64b  */
    gdt[2] = 0x00CF92000000FFFFUL;      /* 0x10 dados do kernel, anel 0        */
    gdt[3] = 0x00CFF2000000FFFFUL;      /* 0x18 dados de usuario, anel 3       */
    gdt[4] = 0x00AFFA000000FFFFUL;      /* 0x20 codigo de usuario, anel 3, 64b */

    memset(&tss, 0, sizeof tss);
    tss.iomap_base = sizeof tss;        /* sem bitmap de E/S: anel 3 nao usa portas */

    uint64_t base = (uint64_t)&tss;
    uint64_t limit = sizeof tss - 1;
    gdt[5] = (limit & 0xFFFF)                   /* 0x28 descritor da TSS (16 bytes) */
           | ((base & 0xFFFFFF) << 16)
           | (0x89UL << 40)                     /* presente, TSS 64 bits disponivel */
           | (((limit >> 16) & 0xF) << 48)
           | (((base >> 24) & 0xFF) << 56);
    gdt[6] = base >> 32;

    struct gdtr gdtr = { sizeof gdt - 1, (uint64_t)gdt };

    /* Carrega a GDT e recarrega CS com um "far return" (nao existe
     * "mov cs"); depois recarrega os segmentos de dados. */
    __asm__ volatile (
        "lgdt %0\n\t"
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n"
        "1:\n\t"
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%ss\n\t"
        "xorw %%ax, %%ax\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        : : "m"(gdtr) : "rax", "memory");

    __asm__ volatile ("ltr %w0" : : "r"((uint16_t)TSS_SEL));
}

void tss_set_kernel_stack(uint64_t rsp0)
{
    tss.rsp0 = rsp0;
}
