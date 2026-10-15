#ifndef MM_VMM_H
#define MM_VMM_H

#include "types.h"

/* Mapa de memoria VIRTUAL de todo processo:
 *   0x00000000 - 0x3FFFFFFF  kernel (1 GiB identidade, so anel 0)
 *   0x40000000 - 0x7FFEFFFF  imagem do programa (codigo e dados)
 *   0x7FFF0000 - 0x7FFFFFFF  pilha do processo (64 KiB)
 * Cada processo tem a SUA tabela de paginas: o mesmo endereco virtual
 * 0x40000000 aponta para paginas fisicas diferentes em cada processo. */
#define USER_BASE        0x40000000UL
#define USER_TOP         0x80000000UL
#define USER_STACK_SIZE  (64 * 1024UL)
#define USER_STACK_TOP   USER_TOP
#define USER_IMAGE_TOP   (USER_STACK_TOP - USER_STACK_SIZE)

void vmm_init(void);
uint64_t vmm_kernel_space(void);
uint64_t vmm_create_space(void);                    /* devolve o CR3 novo */
void vmm_destroy_space(uint64_t pml4);
bool vmm_alloc_range(uint64_t pml4, uint64_t va, uint64_t size);
bool vmm_copy_to(uint64_t pml4, uint64_t va, const void *src, size_t n);
uint64_t vmm_translate(uint64_t pml4, uint64_t va); /* 0 = nao mapeado */
size_t vmm_count_user_pages(uint64_t pml4);

/* Faixas contiguas de enderecos virtuais mapeados (para o comando info). */
struct vmm_range {
    uint64_t start, end;
};
size_t vmm_user_ranges(uint64_t pml4, struct vmm_range *out, size_t max);

#endif
