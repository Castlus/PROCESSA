/* Memoria VIRTUAL (paginacao x86-64 de 4 niveis).
 *
 * Um endereco virtual de 48 bits e quebrado em 4 indices de 9 bits:
 *   PML4 -> PDPT -> PD -> PT -> pagina fisica de 4 KiB
 * O registrador CR3 aponta para a PML4. Trocar o CR3 troca TODO o
 * espaco de enderecamento - e isso que isola um processo do outro.
 *
 * Todo espaco de processo compartilha a mesma tabela do kernel
 * (PDPT[0] -> kernel_pd), sem o bit "usuario", entao o kernel continua
 * acessivel apos a troca de CR3, mas o processo nao consegue toca-lo. */
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "arch/cpu.h"
#include "lib/string.h"

#define PTE_P    0x001UL    /* presente   */
#define PTE_W    0x002UL    /* gravavel   */
#define PTE_U    0x004UL    /* usuario (anel 3 pode acessar) */
#define PTE_PS   0x080UL    /* pagina grande (2 MiB) */
#define PTE_ADDR 0x000FFFFFFFFFF000UL

#define IDX4(va) (((va) >> 39) & 511)
#define IDX3(va) (((va) >> 30) & 511)
#define IDX2(va) (((va) >> 21) & 511)
#define IDX1(va) (((va) >> 12) & 511)

static uint64_t kernel_pml4[512] __attribute__((aligned(4096)));
static uint64_t kernel_pdpt[512] __attribute__((aligned(4096)));
static uint64_t kernel_pd[512]   __attribute__((aligned(4096)));

static bool is_user(uint64_t va)
{
    return va >= USER_BASE && va < USER_TOP;
}

void vmm_init(void)
{
    /* 0 - 1 GiB: endereco virtual == fisico, paginas de 2 MiB, so kernel */
    for (uint64_t i = 0; i < 512; i++)
        kernel_pd[i] = (i << 21) | PTE_P | PTE_W | PTE_PS;
    kernel_pdpt[0] = (uint64_t)kernel_pd | PTE_P | PTE_W;
    kernel_pml4[0] = (uint64_t)kernel_pdpt | PTE_P | PTE_W;
    write_cr3((uint64_t)kernel_pml4);
}

uint64_t vmm_kernel_space(void)
{
    return (uint64_t)kernel_pml4;
}

uint64_t vmm_create_space(void)
{
    uint64_t pml4 = pmm_alloc_page();
    uint64_t pdpt = pmm_alloc_page();
    if (!pml4 || !pdpt) {
        pmm_free_page(pml4);
        pmm_free_page(pdpt);
        return 0;
    }
    ((uint64_t *)pml4)[0] = pdpt | PTE_P | PTE_W | PTE_U;
    ((uint64_t *)pdpt)[0] = (uint64_t)kernel_pd | PTE_P | PTE_W;   /* kernel, sem PTE_U */
    return pml4;
}

static uint64_t *next_table(uint64_t *table, uint64_t idx, bool create)
{
    if (!(table[idx] & PTE_P)) {
        if (!create)
            return NULL;
        uint64_t page = pmm_alloc_page();
        if (!page)
            return NULL;
        table[idx] = page | PTE_P | PTE_W | PTE_U;
    }
    return (uint64_t *)(table[idx] & PTE_ADDR);
}

static uint64_t *find_pte(uint64_t pml4, uint64_t va, bool create)
{
    uint64_t *pdpt = next_table((uint64_t *)pml4, IDX4(va), create);
    if (!pdpt)
        return NULL;
    uint64_t *pd = next_table(pdpt, IDX3(va), create);
    if (!pd)
        return NULL;
    uint64_t *pt = next_table(pd, IDX2(va), create);
    if (!pt)
        return NULL;
    return &pt[IDX1(va)];
}

uint64_t vmm_translate(uint64_t pml4, uint64_t va)
{
    if (!is_user(va))
        return 0;
    uint64_t *pte = find_pte(pml4, va, false);
    if (!pte || !(*pte & PTE_P))
        return 0;
    return (*pte & PTE_ADDR) | (va & (PAGE_SIZE - 1));
}

bool vmm_alloc_range(uint64_t pml4, uint64_t va, uint64_t size)
{
    if (size == 0)
        return true;
    if (!is_user(va) || size > USER_TOP - va)
        return false;

    for (uint64_t page = ALIGN_DOWN(va, PAGE_SIZE); page < va + size; page += PAGE_SIZE) {
        if (vmm_translate(pml4, page))
            continue;                   /* ja mapeada (segmentos vizinhos) */
        uint64_t frame = pmm_alloc_page();
        if (!frame)
            return false;
        uint64_t *pte = find_pte(pml4, page, true);
        if (!pte) {
            pmm_free_page(frame);
            return false;
        }
        *pte = frame | PTE_P | PTE_W | PTE_U;
    }
    return true;
}

bool vmm_copy_to(uint64_t pml4, uint64_t va, const void *src, size_t n)
{
    const uint8_t *s = src;
    while (n) {
        uint64_t pa = vmm_translate(pml4, va);
        if (!pa)
            return false;
        size_t chunk = PAGE_SIZE - (va & (PAGE_SIZE - 1));
        if (chunk > n)
            chunk = n;
        memcpy((void *)pa, s, chunk);   /* fisico == virtual para o kernel */
        va += chunk;
        s += chunk;
        n -= chunk;
    }
    return true;
}

/* Percorre as tabelas do processo (so a parte de usuario, PDPT[1..511]). */
static size_t walk_user(uint64_t pml4, bool destroy)
{
    uint64_t *l4 = (uint64_t *)pml4;
    size_t pages = 0;

    if (!(l4[0] & PTE_P))
        return 0;
    uint64_t *pdpt = (uint64_t *)(l4[0] & PTE_ADDR);
    for (int i = 1; i < 512; i++) {          /* i = 0 e o kernel compartilhado */
        if (!(pdpt[i] & PTE_P))
            continue;
        uint64_t *pd = (uint64_t *)(pdpt[i] & PTE_ADDR);
        for (int j = 0; j < 512; j++) {
            if (!(pd[j] & PTE_P))
                continue;
            uint64_t *pt = (uint64_t *)(pd[j] & PTE_ADDR);
            for (int k = 0; k < 512; k++) {
                if (!(pt[k] & PTE_P))
                    continue;
                pages++;
                if (destroy)
                    pmm_free_page(pt[k] & PTE_ADDR);
            }
            if (destroy)
                pmm_free_page((uint64_t)pt);
        }
        if (destroy)
            pmm_free_page((uint64_t)pd);
    }
    if (destroy)
        pmm_free_page((uint64_t)pdpt);
    return pages;
}

size_t vmm_count_user_pages(uint64_t pml4)
{
    return walk_user(pml4, false);
}

size_t vmm_user_ranges(uint64_t pml4, struct vmm_range *out, size_t max)
{
    uint64_t *l4 = (uint64_t *)pml4;
    size_t n = 0;

    if (!(l4[0] & PTE_P))
        return 0;
    uint64_t *pdpt = (uint64_t *)(l4[0] & PTE_ADDR);
    for (uint64_t i = 1; i < 512; i++) {         /* i = 0 e o kernel */
        if (!(pdpt[i] & PTE_P))
            continue;
        uint64_t *pd = (uint64_t *)(pdpt[i] & PTE_ADDR);
        for (uint64_t j = 0; j < 512; j++) {
            if (!(pd[j] & PTE_P))
                continue;
            uint64_t *pt = (uint64_t *)(pd[j] & PTE_ADDR);
            for (uint64_t k = 0; k < 512; k++) {
                if (!(pt[k] & PTE_P))
                    continue;
                uint64_t va = (i << 30) | (j << 21) | (k << 12);
                if (n && out[n - 1].end == va)
                    out[n - 1].end += PAGE_SIZE;     /* continua a faixa anterior */
                else if (n < max)
                    out[n++] = (struct vmm_range){ va, va + PAGE_SIZE };
                else
                    return n;
            }
        }
    }
    return n;
}

void vmm_destroy_space(uint64_t pml4)
{
    if (!pml4 || pml4 == vmm_kernel_space())
        return;
    walk_user(pml4, true);
    pmm_free_page(pml4);
}
