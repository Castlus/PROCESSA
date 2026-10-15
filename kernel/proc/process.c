/* Tabela de processos: criacao e destruicao de PCBs. */
#include "proc/process.h"
#include "drivers/console.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "lib/string.h"

static struct process table[MAX_PROCS];
static int next_pid = 1;

static const uint8_t palette[] = {
    VGA_LGREEN, VGA_LCYAN, VGA_YELLOW, VGA_LMAGENTA, VGA_LBLUE, VGA_WHITE, VGA_BROWN,
};

struct process *proc_slot(int i)
{
    return &table[i];
}

int proc_index(const struct process *p)
{
    return (int)(p - table);
}

const char *proc_state_name(enum proc_state s)
{
    static const char *const names[] = {
        "LIVRE", "PRONTO", "EXECUTANDO", "DORMINDO", "TERMINADO",
    };
    return names[s];
}

/* PID 0: o proprio kernel parado no laco "hlt" de kmain. O escalonador
 * so escolhe ele quando nenhum outro processo esta pronto. */
struct process *proc_init_idle(void)
{
    struct process *p = &table[0];
    memset(p, 0, sizeof *p);
    p->pid = 0;
    str_copy(p->name, "ocioso", sizeof p->name);
    p->format = "kernel";
    p->cr3 = vmm_kernel_space();
    p->state = P_RUNNING;
    p->color = VGA_DGRAY;
    return p;
}

struct process *proc_find(int pid)
{
    for (int i = 0; i < MAX_PROCS; i++)
        if (table[i].state != P_FREE && table[i].pid == pid)
            return &table[i];
    return NULL;
}

struct process *proc_create(const char *name, const char *format, uint64_t cr3, uint64_t entry)
{
    struct process *p = NULL;
    for (int i = 1; i < MAX_PROCS; i++) {
        if (table[i].state == P_FREE) {
            p = &table[i];
            break;
        }
    }
    if (!p)
        return NULL;

    uint64_t kstack = pmm_alloc_pages(KSTACK_PAGES);
    if (!kstack)
        return NULL;
    if (!vmm_alloc_range(cr3, USER_STACK_TOP - USER_STACK_SIZE, USER_STACK_SIZE)) {
        pmm_free_pages(kstack, KSTACK_PAGES);
        return NULL;
    }

    memset(p, 0, sizeof *p);
    p->pid = next_pid++;
    str_copy(p->name, name, sizeof p->name);
    p->format = format;
    p->cr3 = cr3;
    p->kstack = kstack;
    p->kstack_top = kstack + KSTACK_PAGES * PAGE_SIZE;
    p->entry = entry;
    p->pages = vmm_count_user_pages(cr3);
    p->color = palette[(p->pid - 1) % sizeof palette];
    p->line_start = true;

    /* Contexto inicial: montamos na pilha de kernel um quadro identico ao
     * que uma interrupcao deixaria. Na primeira vez que o escalonador
     * escolher este processo, o "iretq" de isr.asm vai "voltar" para o
     * ponto de entrada do programa, em modo usuario (anel 3). */
    struct regs *r = (struct regs *)(p->kstack_top - sizeof(struct regs));
    memset(r, 0, sizeof *r);
    r->rip = entry;
    r->cs = USER_CS;
    r->rflags = RFLAGS_IF | 0x2;    /* interrupcoes ligadas: o timer pode preemptar */
    r->rsp = USER_STACK_TOP;
    r->ss = USER_DS;
    p->ctx = r;

    p->state = P_READY;
    return p;
}

void proc_kill(struct process *p, int code)
{
    if (p->pid == 0 || p->state == P_FREE || p->state == P_DEAD)
        return;
    p->exit_code = code;
    p->state = P_DEAD;
}

/* Libera memoria de processos mortos. Nunca libera "keep" (o processo
 * em cuja pilha de kernel estamos executando neste momento). */
void proc_reap(const struct process *keep)
{
    for (int i = 1; i < MAX_PROCS; i++) {
        struct process *p = &table[i];
        if (p->state != P_DEAD || p == keep)
            continue;
        vmm_destroy_space(p->cr3);
        pmm_free_pages(p->kstack, KSTACK_PAGES);
        p->state = P_FREE;
    }
}
