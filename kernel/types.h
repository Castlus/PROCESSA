/* Tipos basicos. Nada de <stdint.h>: o kernel nao usa NENHUM cabecalho
 * nem biblioteca do sistema (e compilado com -nostdinc -nostdlib). */
#ifndef TYPES_H
#define TYPES_H

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;
typedef signed char    int8_t;
typedef short          int16_t;
typedef int            int32_t;
typedef long           int64_t;

typedef unsigned long  size_t;
typedef unsigned long  uintptr_t;
typedef _Bool          bool;

#define true  1
#define false 0
#define NULL  ((void *)0)

#define PAGE_SIZE 4096UL
#define ALIGN_DOWN(x, a) ((x) & ~((uint64_t)(a) - 1))
#define ALIGN_UP(x, a)   (((x) + (a) - 1) & ~((uint64_t)(a) - 1))

#endif
