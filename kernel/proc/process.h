#ifndef PROC_PROCESS_H
#define PROC_PROCESS_H

#include "types.h"
#include "arch/cpu.h"

#define MAX_PROCS     16
#define KSTACK_PAGES  4         /* 16 KiB de pilha de kernel por processo */

enum proc_state {
    P_FREE = 0,     /* entrada da tabela livre                        */
    P_READY,        /* pronto: esperando a vez de usar a CPU          */
    P_RUNNING,      /* executando agora                               */
    P_SLEEPING,     /* bloqueado: pediu para dormir (sys_sleep)       */
    P_DEAD,         /* terminou; recursos liberados na proxima troca  */
};

/* =====================================================================
 *  PCB - Process Control Block: E ISTO QUE DEFINE UM PROCESSO.
 *
 *  Um processo = um programa em execucao, com:
 *    - identidade ............ pid, nome, formato do executavel
 *    - estado ................ pronto / executando / dormindo / morto
 *    - contexto de CPU ....... registradores salvos (ctx)
 *    - espaco de enderecamento a SUA tabela de paginas (cr3)
 *    - pilha de kernel ....... usada quando ele e interrompido
 *    - contabilidade ......... tempo de CPU, vezes que foi escalonado
 * ===================================================================== */
struct process {
    int pid;
    enum proc_state state;
    char name[32];
    const char *format;         /* "ELF64", "PE32+", "Mach-O", "RAW", "kernel" */

    struct regs *ctx;           /* contexto salvo (fica na pilha de kernel)    */
    uint64_t cr3;               /* raiz da tabela de paginas (PML4)            */
    uint64_t kstack;            /* base da pilha de kernel                     */
    uint64_t kstack_top;        /* vai para o TSS.rsp0 quando ele roda         */
    uint64_t entry;             /* ponto de entrada do programa                */

    uint64_t wake_us;           /* dormindo: acorda quando uptime >= wake_us   */
    uint64_t cpu_us;            /* tempo de CPU consumido (microssegundos)     */
    uint64_t switches;          /* quantas vezes recebeu a CPU                 */
    size_t pages;               /* paginas de memoria de usuario               */
    int exit_code;

    uint8_t color;              /* cor da saida na tela */
    bool line_start;            /* para prefixar cada linha com [pid:nome]     */
};

struct process *proc_init_idle(void);
struct process *proc_create(const char *name, const char *format, uint64_t cr3, uint64_t entry);
void proc_kill(struct process *p, int code);
void proc_reap(const struct process *keep);
struct process *proc_find(int pid);
struct process *proc_slot(int i);
int proc_index(const struct process *p);
const char *proc_state_name(enum proc_state s);

#endif
