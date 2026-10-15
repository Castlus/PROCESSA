/* Chamadas de sistema: a unica porta de entrada de um processo no kernel. */
#include "proc/syscall.h"
#include "proc/process.h"
#include "proc/sched.h"
#include "drivers/console.h"
#include "mm/vmm.h"
#include "lib/printf.h"

#define WRITE_MAX 1024

static bool output_on_screen = true;

void syscall_set_output(bool on) { output_on_screen = on; }
bool syscall_output(void)        { return output_on_screen; }

static void out(const char *s, size_t n, uint8_t color)
{
    if (output_on_screen)
        console_write(s, n, color);
    else
        console_write_serial(s, n);
}

/* Nunca confie num ponteiro vindo do processo: confere se o buffer esta
 * inteiro na area de usuario E mapeado na tabela de paginas dele. */
static bool user_buffer_ok(const struct process *p, uint64_t addr, uint64_t len)
{
    if (len == 0)
        return true;
    if (addr < USER_BASE || addr >= USER_TOP || len > USER_TOP - addr)
        return false;
    for (uint64_t va = ALIGN_DOWN(addr, PAGE_SIZE); va < addr + len; va += PAGE_SIZE)
        if (!vmm_translate(p->cr3, va))
            return false;
    return true;
}

/* Escreve no log com a cor do processo, prefixando cada linha com [pid:nome]. */
static void write_from(struct process *p, const char *s, size_t n)
{
    char prefix[48];
    size_t i = 0;
    while (i < n) {
        if (p->line_start) {
            int k = ksnprintf(prefix, sizeof prefix, "[%d:%s] ", p->pid, p->name);
            out(prefix, (size_t)k, p->color);
            p->line_start = false;
        }
        size_t j = i;
        while (j < n && s[j] != '\n')
            j++;
        if (j < n) {
            j++;                        /* inclui o '\n' */
            p->line_start = true;
        }
        out(s + i, j - i, p->color);
        i = j;
    }
}

void syscall_dispatch(struct regs *r)
{
    struct process *p = sched_current();

    switch (r->rax) {
    case SYS_EXIT:
        kprintf("[kernel] PID %d (%s) terminou com código %d\n", p->pid, p->name, (int)r->rdi);
        proc_kill(p, (int)r->rdi);
        break;

    case SYS_WRITE: {
        uint64_t len = r->rsi > WRITE_MAX ? WRITE_MAX : r->rsi;
        if (!user_buffer_ok(p, r->rdi, len)) {
            r->rax = (uint64_t)-1;
            break;
        }
        write_from(p, (const char *)r->rdi, len);   /* CR3 atual = do processo */
        r->rax = len;
        break;
    }

    case SYS_GETPID:
        r->rax = (uint64_t)p->pid;
        break;

    case SYS_YIELD:                     /* devolve a CPU voluntariamente */
        sched_yield();
        r->rax = 0;
        break;

    case SYS_SLEEP:                     /* bloqueia por N milissegundos */
        sched_sleep_ms(r->rdi);
        r->rax = 0;
        break;

    case SYS_UPTIME:
        r->rax = sched_uptime_ms();
        break;

    default:
        r->rax = (uint64_t)-1;
        break;
    }
}
