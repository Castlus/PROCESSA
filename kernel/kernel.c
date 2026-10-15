/* =====================================================================
 *  PROCESSA - kmain: inicializacao do kernel
 * ===================================================================== */
#include "types.h"
#include "boot.h"
#include "arch/gdt.h"
#include "arch/io.h"
#include "arch/idt.h"
#include "drivers/ata.h"
#include "drivers/console.h"
#include "drivers/keyboard.h"
#include "drivers/lapic.h"
#include "drivers/pic.h"
#include "drivers/serial.h"
#include "drivers/timer.h"
#include "fs/procfs.h"
#include "loader/loader.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "proc/sched.h"
#include "lib/printf.h"
#include "shell.h"
#include "ui.h"

#define TIMER_HZ 100

static bool keyboard_polling;

/* Chamado pelo driver do timer a cada interrupcao. */
static void on_timer_tick(void)
{
    if (keyboard_polling)
        keyboard_poll();    /* boot UEFI: o teclado e lido a cada tick */
    sched_tick();           /* contabiliza o tempo e decide se e hora de trocar */
    ui_tick();              /* atualiza o painel */
}

/* Liga as interrupcoes e espera ~0,3 s pelo primeiro tick. Em PCs reais o
 * chip do timer pode estar desligado pelo firmware; sem ele nada alterna,
 * entao e melhor avisar na tela do que parecer travado. Se o tick chegar,
 * o escalonador ja troca para os processos e este laco so termina depois. */
static void check_timer(void)
{
    uint64_t start = timer_ticks();
    __asm__ volatile ("sti");
    for (int i = 0; i < 300000 && timer_ticks() == start; i++)
        io_wait();                      /* ~1 microssegundo cada */
    if (timer_ticks() == start)
        kprintf_color(COLOR_ERROR, "[timer] ATENÇÃO: o timer não gerou nenhuma interrupção! "
                      "Sem ele os processos não se alternam.\n");
}

static void log_boot(void)
{
    if (boot.kind == BOOT_UEFI) {
        const struct uefi_bootinfo *u = boot.uefi;
        kprintf("[boot]  UEFI, firmware \"%s\"; tela %ux%u; TSC %lu MHz\n",
                u->firmware, u->fb_width, u->fb_height, u->tsc_hz / 1000000);
    } else if (boot.ramdisk) {
        kprintf("[boot]  BIOS deu boot pelo disco %02x e copiou %u KiB de arquivos para a RAM\n",
                boot.bios_drive, boot.ramdisk_sectors / 2);
    }
}

void kmain(uint64_t boot_arg)
{
    boot_parse(boot_arg);       /* BIOS ou UEFI? o que o carregador deixou? */
    vmm_init();                 /* tabelas de paginas do kernel */
    serial_init();
    console_init();             /* modo texto (BIOS) ou tela grafica (UEFI) */
    ui_init();
    kprintf_color(COLOR_TITLE, "PROCESSA - kernel x86-64 rodando em modo longo (64 bits)\n");
    log_boot();

    gdt_init();
    kprintf("[cpu]   GDT: segmentos de kernel (anel 0) e de usuário (anel 3) + TSS\n");
    idt_init();
    kprintf("[cpu]   IDT: exceções, IRQs, timer do APIC e chamada de sistema (int 0x80)\n");
    pic_init();
    kprintf("[pic]   8259 remapeado: IRQ 0-15 -> vetores 32-47\n");

    if (boot.kind == BOOT_UEFI)
        pmm_init_ranges(boot.uefi->mem, boot.uefi->mem_count);
    else
        pmm_init();
    kprintf("[mem]   %lu páginas físicas livres (%lu MiB); usuário em 0x40000000\n",
            pmm_total_pages(), pmm_total_pages() * 4 / 1024);

    sched_init();
    bool uefi = boot.kind == BOOT_UEFI;
    timer_init(TIMER_HZ, on_timer_tick, uefi, uefi ? boot.uefi->tsc_hz : 0);
    if (timer_is_lapic())
        kprintf("[timer] timer do APIC local%s em %u Hz (calibrado: %lu MHz) -> vetor 0x40\n",
                lapic_x2apic() ? " (x2APIC)" : "", timer_frequency(), lapic_timer_hz() / 1000000);
    else
        kprintf("[timer] PIT 8254 em %u Hz (divisor %u) -> IRQ0\n", timer_frequency(), pit_divisor());
    kprintf("[sched] round-robin, quantum de %u ticks\n", sched_quantum());

    keyboard_polling = timer_is_lapic();
    keyboard_init(shell_key, !keyboard_polling);
    kprintf(keyboard_polling ? "[tecl]  teclado PS/2 lido a cada tick do timer\n"
                             : "[tecl]  teclado PS/2 -> IRQ1\n");

    bool have_ata = ata_init();
    if (have_ata)
        kprintf("[disco] ATA PIO: \"%s\", %u MiB\n", ata_model(), ata_sectors() / 2048);
    else
        kprintf("[disco] nenhum disco ATA (IDE) nesta máquina\n");
    if (!fs_init(have_ata))
        panic("PROCFS não encontrado (nem no disco ATA, nem na cópia feita no boot)");
    kprintf("[fs]    PROCFS: %d arquivo(s), lidos de: %s\n", fs_count(), fs_source());

    for (int i = 0; i < fs_count(); i++) {
        const struct fs_entry *f = fs_entry_at(i);
        if (f->flags & FS_FLAG_AUTORUN)
            loader_exec(f->name);
    }

    shell_init();
    kprintf_color(COLOR_TITLE, "[kernel] interrupções ligadas: o timer agora alterna os processos\n");
    check_timer();

    /* Daqui em diante este laco e o processo "ocioso" (PID 0). Todo o resto
     * acontece dentro das interrupcoes: timer, teclado e chamadas de sistema. */
    for (;;)
        __asm__ volatile ("sti; hlt");
}
