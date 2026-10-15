#ifndef LIB_STDARG_H
#define LIB_STDARG_H

/* Argumentos variaveis (printf). Nao e biblioteca: sao "builtins" do
 * proprio compilador, que sabe em quais registradores/pilha os
 * argumentos chegaram. */
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

#endif
