/* =====================================================================
 *  PROCESSA - kmain: inicializacao do kernel
 * ===================================================================== */
#include "types.h"
#include "arch/gdt.h"
#include "arch/idt.h"
#include "drivers/ata.h"
#include "drivers/console.h"
#include "drivers/keyboard.h"
#include "drivers/pic.h"
#include "drivers/pit.h"
#include "drivers/serial.h"
#include "fs/procfs.h"
#include "loader/loader.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "proc/sched.h"
#include "lib/printf.h"
#include "shell.h"
#include "ui.h"

#define TIMER_HZ 100

/* Chamado pelo driver do timer a cada interrupcao (IRQ0). */
static void on_timer_tick(void)
{
    sched_tick();   /* contabiliza o tempo e decide se e hora de trocar */
    ui_tick();      /* atualiza o painel */
}

void kmain(void)
{
    serial_init();
    console_init();
    ui_init();
    kprintf_color(COLOR_TITLE, "PROCESSA - kernel x86-64 rodando em modo longo (64 bits)\n");

    gdt_init();
    kprintf("[cpu]   GDT: segmentos de kernel (anel 0) e de usuário (anel 3) + TSS\n");
    idt_init();
    kprintf("[cpu]   IDT: 32 exceções, 16 IRQs e chamada de sistema (int 0x80)\n");
    pic_init();
    kprintf("[pic]   8259 remapeado: IRQ 0-15 -> vetores 32-47\n");

    pmm_init();
    vmm_init();
    kprintf("[mem]   %lu páginas físicas livres (%lu MiB); usuário em 0x40000000\n",
            pmm_total_pages(), pmm_total_pages() * 4 / 1024);

    sched_init();
    pit_init(TIMER_HZ, on_timer_tick);
    kprintf("[timer] PIT 8254 em %u Hz (divisor %u) -> IRQ0; quantum %u ticks\n",
            pit_frequency(), pit_divisor(), sched_quantum());
    keyboard_init(shell_key);
    kprintf("[tecl]  teclado PS/2 -> IRQ1\n");

    if (!ata_init())
        panic("disco ATA não encontrado");
    kprintf("[disco] ATA PIO: \"%s\", %u MiB\n", ata_model(), ata_sectors() / 2048);
    if (!fs_init())
        panic("diretório PROCFS não encontrado no disco");
    kprintf("[fs]    PROCFS: %d arquivo(s) no disco\n", fs_count());

    for (int i = 0; i < fs_count(); i++) {
        const struct fs_entry *f = fs_entry_at(i);
        if (f->flags & FS_FLAG_AUTORUN)
            loader_exec(f->name);
    }

    shell_init();
    kprintf_color(COLOR_TITLE, "[kernel] interrupções ligadas: o timer agora alterna os processos\n");

    /* Daqui em diante este laco e o processo "ocioso" (PID 0). Todo o resto
     * acontece dentro das interrupcoes: timer, teclado e chamadas de sistema. */
    for (;;)
        __asm__ volatile ("sti; hlt");
}
