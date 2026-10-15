#ifndef PROC_SCHED_H
#define PROC_SCHED_H

#include "types.h"
#include "arch/cpu.h"
#include "proc/process.h"

#define DEFAULT_QUANTUM 5       /* ticks do timer por fatia de tempo */

void sched_init(void);
void sched_tick(void);                  /* chamado pelo driver do timer */
void sched_save_context(struct regs *r);
uint64_t sched_switch(void);            /* devolve o contexto a retomar */

struct process *sched_current(void);
void sched_yield(void);
void sched_sleep_ms(uint64_t ms);

void sched_set_quantum(uint32_t ticks);
uint32_t sched_quantum(void);
uint32_t sched_slice_left(void);        /* ticks que restam na fatia atual */
uint64_t sched_switches(void);
uint64_t sched_uptime_ms(void);
int sched_ready_queue(int *pids, int max);

void sched_set_trace(bool on);          /* registra cada troca no log */
bool sched_trace(void);

#endif
