/* Shell de comandos na ultima linha da tela. Roda dentro da interrupcao
 * do teclado (IRQ1): cada tecla chega por shell_key(). */
#include "shell.h"
#include "drivers/ata.h"
#include "drivers/console.h"
#include "drivers/pit.h"
#include "fs/procfs.h"
#include "loader/loader.h"
#include "mm/pmm.h"
#include "proc/process.h"
#include "proc/sched.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "ui.h"

#define PROMPT     "processa> "
#define PROMPT_LEN 10
#define LINE_MAX   60
#define MAX_ARGS   4

static char line[LINE_MAX + 1];
static int line_len;

static void redraw(void)
{
    line[line_len] = '\0';
    vga_write_at(PROMPT_ROW, 0, PROMPT, COLOR_PROMPT, PROMPT_LEN);
    vga_write_at(PROMPT_ROW, PROMPT_LEN, line, COLOR_INPUT, VGA_COLS - PROMPT_LEN);
    vga_set_cursor(PROMPT_ROW, PROMPT_LEN + line_len);
}

static void cmd_help(void)
{
    kprintf_color(COLOR_TITLE,
                  "comandos:\n"
                  "  ls             arquivos do disco e o formato detectado de cada um\n"
                  "  run <arquivo>  carrega o arquivo do disco e cria um processo\n"
                  "  kill <pid>     encerra um processo\n"
                  "  ps             lista os processos (PCBs) no log\n"
                  "  quantum <n>    fatia de tempo, em ticks do timer (1-100)\n"
                  "  freq <hz>      frequência do timer PIT (19-1000 Hz)\n"
                  "  mem            memória física em uso\n"
                  "  limpar         limpa o log\n");
}

static void cmd_ls(void)
{
    static uint8_t head[SECTOR_SIZE];
    kprintf_color(COLOR_TITLE, "%-18s %8s  %-7s %s\n", "ARQUIVO", "BYTES", "FORMATO", "ORIGEM");
    for (int i = 0; i < fs_count(); i++) {
        const struct fs_entry *f = fs_entry_at(i);
        const char *name = "?", *origin = "erro de leitura";
        if (fs_read_head(f, head)) {
            const struct binfmt *b = loader_detect(head, f->size < sizeof head ? f->size : sizeof head);
            name = b->name;
            origin = b->origin;
        }
        kprintf("%-18s %8u  %-7s %s%s\n", f->name, f->size, name, origin,
                (f->flags & FS_FLAG_AUTORUN) ? "  [inicia no boot]" : "");
    }
}

static void cmd_ps(void)
{
    kprintf_color(COLOR_TITLE, "%4s %-16s %-7s %-10s %9s %7s\n",
                  "PID", "NOME", "FORMATO", "ESTADO", "CPU(ms)", "TROCAS");
    for (int i = 0; i < MAX_PROCS; i++) {
        struct process *p = proc_slot(i);
        if (p->state == P_FREE)
            continue;
        kprintf("%4d %-16.16s %-7s %-10s %9lu %7lu\n", p->pid, p->name, p->format,
                proc_state_name(p->state), p->cpu_us / 1000, p->switches);
    }
}

static void cmd_kill(const char *arg)
{
    long pid = str_to_long(arg);
    struct process *p = pid > 0 ? proc_find((int)pid) : NULL;
    if (!p || p->state == P_DEAD) {
        kprintf_color(COLOR_ERROR, "kill: processo '%s' não existe (o PID 0 é o kernel)\n",
                      arg ? arg : "");
        return;
    }
    kprintf("[kernel] PID %d (%s) encerrado pelo usuário\n", p->pid, p->name);
    proc_kill(p, -9);
}

static void cmd_quantum(const char *arg)
{
    long n = str_to_long(arg);
    if (n < 1 || n > 100) {
        kprintf_color(COLOR_ERROR, "uso: quantum <1-100>   (atual: %u ticks)\n", sched_quantum());
        return;
    }
    sched_set_quantum((uint32_t)n);
    kprintf("[sched] quantum = %ld ticks = %lu ms por fatia\n", n,
            (uint64_t)n * 1000 / pit_frequency());
}

static void cmd_freq(const char *arg)
{
    long hz = str_to_long(arg);
    if (hz < PIT_MIN_HZ || hz > PIT_MAX_HZ) {
        kprintf_color(COLOR_ERROR, "uso: freq <%d-%d>   (atual: %u Hz)\n",
                      PIT_MIN_HZ, PIT_MAX_HZ, pit_frequency());
        return;
    }
    pit_set_frequency((uint32_t)hz);
    kprintf("[timer] PIT reprogramado: %u Hz (divisor %u), tick a cada %u us\n",
            pit_frequency(), pit_divisor(), 1000000 / pit_frequency());
}

static void cmd_mem(void)
{
    size_t used = pmm_used_pages(), total = pmm_total_pages();
    kprintf("[mem] %lu de %lu páginas em uso (%lu KiB usados, %lu KiB livres)\n",
            used, total, used * 4, (total - used) * 4);
}

static int split(char *s, char **argv)
{
    int argc = 0;
    while (*s && argc < MAX_ARGS) {
        while (*s == ' ')
            s++;
        if (!*s)
            break;
        argv[argc++] = s;
        while (*s && *s != ' ')
            s++;
        if (*s)
            *s++ = '\0';
    }
    return argc;
}

static void execute(char *cmd)
{
    char *argv[MAX_ARGS] = { 0 };
    int argc = split(cmd, argv);
    if (!argc)
        return;

    const char *c = argv[0];
    if (!strcmp(c, "ajuda") || !strcmp(c, "help"))
        cmd_help();
    else if (!strcmp(c, "ls"))
        cmd_ls();
    else if (!strcmp(c, "run") || !strcmp(c, "exec")) {
        if (argc < 2)
            kprintf_color(COLOR_ERROR, "uso: run <arquivo>   (veja os arquivos com 'ls')\n");
        else if (loader_exec(argv[1]))
            ui_refresh();
    } else if (!strcmp(c, "kill"))
        cmd_kill(argv[1]);
    else if (!strcmp(c, "ps"))
        cmd_ps();
    else if (!strcmp(c, "quantum"))
        cmd_quantum(argv[1]);
    else if (!strcmp(c, "freq"))
        cmd_freq(argv[1]);
    else if (!strcmp(c, "mem"))
        cmd_mem();
    else if (!strcmp(c, "limpar") || !strcmp(c, "clear"))
        console_clear_log();
    else
        kprintf_color(COLOR_ERROR, "comando desconhecido: '%s' (digite 'ajuda')\n", c);
}

void shell_key(char c)
{
    if (c == '\n') {
        line[line_len] = '\0';
        kprintf_color(COLOR_PROMPT, "%s%s\n", PROMPT, line);
        execute(line);
        line_len = 0;
    } else if (c == '\b') {
        if (line_len)
            line_len--;
    } else if (c == 27) {
        line_len = 0;
    } else if (c >= 32 && c < 127 && line_len < LINE_MAX) {
        line[line_len++] = c;
    }
    redraw();
}

void shell_init(void)
{
    line_len = 0;
    redraw();
    kprintf_color(COLOR_PROMPT, "digite 'ajuda' para ver os comandos\n");
}
