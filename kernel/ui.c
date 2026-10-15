/* Painel ao vivo: barra de status + tabela de processos (le os PCBs).
 * E redesenhado pela interrupcao do timer. */
#include "ui.h"
#include "drivers/console.h"
#include "drivers/pit.h"
#include "proc/process.h"
#include "proc/sched.h"
#include "proc/syscall.h"
#include "lib/printf.h"
#include "lib/string.h"

#define STATUS_ROW 0
#define HEADER_ROW 1
#define TABLE_TOP  2
#define TABLE_ROWS 7
#define SEP_ROW    9
#define ROW_FMT    "%c%3s %-16.16s %-7s %-10s %8s %7s %7s  %s"

static uint64_t last_refresh_ms;

static void draw_status(void)
{
    char buf[100];
    struct process *p = sched_current();
    uint64_t ms = sched_uptime_ms();
    ksnprintf(buf, sizeof buf,
              " PROCESSA | %lu.%lus | PIT %uHz | quantum %u | trocas %lu | CPU: %d %.12s",
              ms / 1000, (ms / 100) % 10, pit_frequency(), sched_quantum(),
              sched_switches(), p->pid, p->name);
    vga_write_at(STATUS_ROW, 0, buf, COLOR_STATUS, VGA_COLS);
}

/* Linha separadora: mostra o driver do timer trabalhando (ticks e a
 * fatia de tempo que ainda resta ao processo na CPU). */
static void draw_timer(void)
{
    char buf[80];
    uint32_t left = sched_slice_left();
    const char *tr = sched_trace() ? "on" : "off";
    const char *out = syscall_output() ? "" : " | saida off";

    if (left)
        ksnprintf(buf, sizeof buf, " tick %lu | fatia: resta %u de %u | trace %s%s ",
                  pit_ticks(), left, sched_quantum(), tr, out);
    else
        ksnprintf(buf, sizeof buf, " tick %lu | CPU ociosa | trace %s%s ", pit_ticks(), tr, out);

    vga_fill_row(SEP_ROW, 0xC4, COLOR_SEP);     /* 0xC4 = linha horizontal na CP437 */
    vga_write_at(SEP_ROW, 2, " log ", COLOR_SEP, 0);
    vga_write_at(SEP_ROW, VGA_COLS - 2 - (int)strlen(buf), buf, COLOR_KERNEL, 0);
}

void ui_refresh(void)
{
    char buf[100], pid[8], cpu[16], sw[16], mem[16], cr3[24];
    struct process *cur = sched_current();
    int row = TABLE_TOP, hidden = 0;

    for (int i = 0; i < MAX_PROCS; i++) {
        struct process *p = proc_slot(i);
        if (p->state == P_FREE)
            continue;
        if (row >= TABLE_TOP + TABLE_ROWS) {
            hidden++;
            continue;
        }
        ksnprintf(pid, sizeof pid, "%d", p->pid);
        ksnprintf(cpu, sizeof cpu, "%lu", p->cpu_us / 1000);
        ksnprintf(sw, sizeof sw, "%lu", p->switches);
        ksnprintf(mem, sizeof mem, "%lu", p->pages * 4);
        ksnprintf(cr3, sizeof cr3, "%08lx", p->cr3);
        ksnprintf(buf, sizeof buf, ROW_FMT, p == cur ? '>' : ' ', pid, p->name,
                  p->format, proc_state_name(p->state), cpu, sw, mem, cr3);
        vga_write_at(row++, 0, buf, p == cur ? COLOR_RUNNING : p->color, VGA_COLS);
    }
    while (row < TABLE_TOP + TABLE_ROWS)
        vga_fill_row(row++, ' ', COLOR_KERNEL);
    if (hidden) {
        ksnprintf(buf, sizeof buf, "(+%d)", hidden);
        vga_write_at(TABLE_TOP + TABLE_ROWS - 1, 74, buf, COLOR_SEP, 6);
    }
}

void ui_init(void)
{
    char buf[100];
    ksnprintf(buf, sizeof buf, ROW_FMT, ' ', "PID", "NOME", "FORMATO", "ESTADO",
              "CPU(ms)", "TROCAS", "MEM(KB)", "CR3");
    vga_write_at(HEADER_ROW, 0, buf, COLOR_HEADER, VGA_COLS);
    draw_status();
    draw_timer();
    ui_refresh();
}

void ui_tick(void)
{
    uint64_t now = sched_uptime_ms();
    if (now - last_refresh_ms >= 20) {          /* painel: ate 50x por segundo */
        last_refresh_ms = now;
        draw_status();
        draw_timer();
        ui_refresh();
    }
}
