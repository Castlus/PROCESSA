#ifndef PROC_SYSCALL_H
#define PROC_SYSCALL_H

#include "arch/cpu.h"

/* Chamadas de sistema (int 0x80). Numero em rax, argumentos em rdi, rsi;
 * retorno em rax. Mesma tabela em user/include/processa.h. */
enum {
    SYS_EXIT   = 0,
    SYS_WRITE  = 1,
    SYS_GETPID = 2,
    SYS_YIELD  = 3,
    SYS_SLEEP  = 4,
    SYS_UPTIME = 5,
};

void syscall_dispatch(struct regs *r);

/* Saida dos processos na tela (o log serial sempre recebe tudo). */
void syscall_set_output(bool on);
bool syscall_output(void);

#endif
