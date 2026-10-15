/* processa.h - interface entre os programas e o kernel PROCESSA.
 * Nao e libc: sao so 6 chamadas de sistema feitas com "int 0x80".
 * Numero da chamada em rax, argumentos em rdi e rsi, retorno em rax. */
#ifndef PROCESSA_H
#define PROCESSA_H

#define SYS_EXIT   0
#define SYS_WRITE  1
#define SYS_GETPID 2
#define SYS_YIELD  3
#define SYS_SLEEP  4
#define SYS_UPTIME 5

static inline long syscall2(long n, long a, long b)
{
    long ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(n), "D"(a), "S"(b) : "memory");
    return ret;
}

static inline __attribute__((noreturn)) void sys_exit(int code)
{
    syscall2(SYS_EXIT, code, 0);
    for (;;) {}
}

static inline long sys_write(const char *buf, unsigned long len)
{
    return syscall2(SYS_WRITE, (long)buf, (long)len);
}

static inline long sys_getpid(void)               { return syscall2(SYS_GETPID, 0, 0); }
static inline void sys_yield(void)                { syscall2(SYS_YIELD, 0, 0); }
static inline void sys_sleep(unsigned long ms)    { syscall2(SYS_SLEEP, (long)ms, 0); }
static inline unsigned long sys_uptime(void)      { return (unsigned long)syscall2(SYS_UPTIME, 0, 0); }

/* user/lib/ulib.c */
unsigned long strlen(const char *s);
void *memset(void *dst, int c, unsigned long n);
void *memcpy(void *dst, const void *src, unsigned long n);
void print(const char *s);
void printf(const char *fmt, ...);

#endif
