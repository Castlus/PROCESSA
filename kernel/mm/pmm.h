#ifndef MM_PMM_H
#define MM_PMM_H

#include "types.h"

void pmm_init(void);
uint64_t pmm_alloc_pages(size_t n);     /* n paginas contiguas, zeradas; 0 = sem memoria */
void pmm_free_pages(uint64_t addr, size_t n);
uint64_t pmm_alloc_page(void);
void pmm_free_page(uint64_t addr);
size_t pmm_used_pages(void);
size_t pmm_total_pages(void);

#endif
