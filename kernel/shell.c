/* Shell de comandos na ultima linha da tela. Roda dentro da interrupcao
 * do teclado (IRQ1): cada tecla chega por shell_key(). */
#include "shell.h"
#include "drivers/ata.h"
#include "drivers/console.h"
#include "drivers/timer.h"
#include "fs/procfs.h"
#include "loader/loader.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "proc/process.h"
#include "proc/sched.h"
#include "proc/syscall.h"
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
                  "  ps             lista os processos e a fila de prontos\n"
                  "  info <pid>     mostra o PCB inteiro: registradores salvos e memória\n"
                  "  trace on|off   registra cada troca de contexto no log\n"
                  "  saida on|off   mostra/esconde a saída dos processos na tela\n"
                  "  quantum <n>    fatia de tempo, em ticks do timer (1-100)\n"
                  "  freq <hz>      frequência do timer (19-1000 Hz)\n"
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

    int queue[MAX_PROCS];
    int n = sched_ready_queue(queue, MAX_PROCS);
    kprintf("fila de prontos (ordem do round-robin): ");
    if (!n)
        kprintf("vazia");
    for (int i = 0; i < n; i++)
        kprintf(i ? " -> %d" : "%d", queue[i]);
    kprintf("\n");
}

/* Mostra o PCB inteiro: e a "prova" de que o processo pausado e so isto. */
static void cmd_info(const char *arg)
{
    long pid = str_to_long(arg);
    struct process *p = pid >= 0 ? proc_find((int)pid) : NULL;
    if (!p) {
        kprintf_color(COLOR_ERROR, "uso: info <pid>   (veja os PIDs com 'ps')\n");
        return;
    }

    kprintf_color(COLOR_TITLE, "PCB do PID %d (%s): %s, formato %s\n",
                  p->pid, p->name, proc_state_name(p->state), p->format);
    if (p->pid == 0)
        kprintf("  é o próprio kernel no laço sti;hlt | CR3 %08lx | pilha de boot\n", p->cr3);
    else
        kprintf("  entrada %lx | CR3 %08lx | pilha de kernel %08lx-%08lx\n",
                p->entry, p->cr3, p->kstack, p->kstack_top);
    kprintf("  CPU %lu ms | escalonado %lu vezes | memória %lu KiB",
            p->cpu_us / 1000, p->switches, p->pages * 4);
    if (p->state == P_SLEEPING) {
        uint64_t wake_ms = p->wake_us / 1000, now = sched_uptime_ms();
        kprintf(" | acorda em %lu ms", wake_ms > now ? wake_ms - now : 0);
    }
    kprintf("\n");

    const struct regs *r = p->ctx;
    if (r) {
        kprintf("  contexto salvo (de onde ele continua), anel %lu:\n", r->cs & 3);
        kprintf("   rip=%016lx rsp=%016lx rflags=%03lx cs=%02lx ss=%02lx\n",
                r->rip, r->rsp, r->rflags, r->cs, r->ss);
        kprintf("   rax=%016lx rbx=%016lx rcx=%016lx\n", r->rax, r->rbx, r->rcx);
        kprintf("   rdx=%016lx rsi=%016lx rdi=%016lx\n", r->rdx, r->rsi, r->rdi);
        kprintf("   rbp=%016lx r8 =%016lx r9 =%016lx\n", r->rbp, r->r8, r->r9);
        kprintf("   r10=%016lx r11=%016lx r12=%016lx\n", r->r10, r->r11, r->r12);
        kprintf("   r13=%016lx r14=%016lx r15=%016lx\n", r->r13, r->r14, r->r15);
    }

    struct vmm_range ranges[8];
    size_t n = vmm_user_ranges(p->cr3, ranges, 8);
    if (!n)
        kprintf("  memória de usuário: nenhuma (roda dentro do kernel)\n");
    for (size_t i = 0; i < n; i++)
        kprintf("  memória %08lx-%08lx %4lu KiB  %s\n", ranges[i].start, ranges[i].end,
                (ranges[i].end - ranges[i].start) / 1024,
                ranges[i].end == USER_STACK_TOP ? "pilha" : "programa");
}

static void cmd_trace(const char *arg)
{
    bool on = arg ? !strcmp(arg, "on") : !sched_trace();
    if (arg && !on && strcmp(arg, "off")) {
        kprintf_color(COLOR_ERROR, "uso: trace on|off\n");
        return;
    }
    sched_set_trace(on);
    if (on)
        kprintf("[trace] ligado: cada troca de contexto aparece no log. "
                "Para ver devagar: freq 19 e quantum 20\n");
    else
        kprintf("[trace] desligado\n");
}

static void cmd_saida(const char *arg)
{
    bool on = arg ? !strcmp(arg, "on") : !syscall_output();
    if (arg && !on && strcmp(arg, "off")) {
        kprintf_color(COLOR_ERROR, "uso: saida on|off\n");
        return;
    }
    syscall_set_output(on);
    if (on)
        kprintf("[saida] processos voltam a escrever na tela\n");
    else
        kprintf("[saida] tela só para o kernel; os processos continuam rodando "
                "(a saída deles segue no log serial)\n");
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
            (uint64_t)n * 1000 / timer_frequency());
}

static void cmd_freq(const char *arg)
{
    long hz = str_to_long(arg);
    if (hz < TIMER_MIN_HZ || hz > TIMER_MAX_HZ) {
        kprintf_color(COLOR_ERROR, "uso: freq <%d-%d>   (atual: %u Hz)\n",
                      TIMER_MIN_HZ, TIMER_MAX_HZ, timer_frequency());
        return;
    }
    timer_set_frequency((uint32_t)hz);
    if (timer_is_lapic())
        kprintf("[timer] timer do APIC reprogramado: %u Hz, tick a cada %u us\n",
                timer_frequency(), 1000000 / timer_frequency());
    else
        kprintf("[timer] PIT reprogramado: %u Hz (divisor %u), tick a cada %u us\n",
                timer_frequency(), pit_divisor(), 1000000 / timer_frequency());
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
    else if (!strcmp(c, "info"))
        cmd_info(argv[1]);
    else if (!strcmp(c, "trace"))
        cmd_trace(argv[1]);
    else if (!strcmp(c, "saida"))
        cmd_saida(argv[1]);
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
