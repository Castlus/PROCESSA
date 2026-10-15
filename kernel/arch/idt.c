/* IDT (Interrupt Descriptor Table): para cada numero de interrupcao
 * (0-255), diz a CPU qual codigo executar. */
#include "arch/idt.h"
#include "arch/cpu.h"

struct idt_entry {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_hi;
    uint32_t zero;
} __attribute__((packed));

struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

#define GATE_KERNEL 0x8E    /* presente, DPL 0, interrupt gate 64 bits           */
#define GATE_USER   0xEE    /* presente, DPL 3: modo usuario pode chamar com int */

static struct idt_entry idt[256] __attribute__((aligned(16)));

extern uint64_t isr_stub_table[48];     /* isr.asm */
extern void isr_stub_128(void);
extern void isr_stub_64(void);
extern void isr_stub_255(void);
extern void isr_stub_other(void);

static void set_gate(int vec, uint64_t handler, uint8_t attr)
{
    idt[vec].offset_lo  = handler & 0xFFFF;
    idt[vec].selector   = KERNEL_CS;
    idt[vec].ist        = 0;
    idt[vec].type_attr  = attr;
    idt[vec].offset_mid = (handler >> 16) & 0xFFFF;
    idt[vec].offset_hi  = (uint32_t)(handler >> 32);
    idt[vec].zero       = 0;
}

void idt_init(void)
{
    for (int i = 48; i < 256; i++)              /* nenhum vetor fica sem tratador */
        set_gate(i, (uint64_t)isr_stub_other, GATE_KERNEL);
    for (int i = 0; i < 48; i++)
        set_gate(i, isr_stub_table[i], GATE_KERNEL);
    set_gate(LAPIC_TIMER_VECTOR, (uint64_t)isr_stub_64, GATE_KERNEL);
    set_gate(LAPIC_SPURIOUS_VECTOR, (uint64_t)isr_stub_255, GATE_KERNEL);
    set_gate(SYSCALL_VECTOR, (uint64_t)isr_stub_128, GATE_USER);

    struct idtr idtr = { sizeof idt - 1, (uint64_t)idt };
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}
