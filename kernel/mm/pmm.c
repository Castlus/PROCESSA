/* Gerenciador de memoria FISICA: divide a RAM em paginas de 4 KiB e
 * marca num mapa de bits quais estao livres (0) ou em uso (1).
 *
 * Abaixo de 2 MiB fica o kernel, a BIOS e a memoria de video, entao o
 * alocador so entrega paginas de 2 MiB a 64 MiB (o QEMU roda com 128 MiB).
 * Como o kernel mapeia a memoria fisica 1:1, o endereco fisico devolvido
 * tambem e um ponteiro valido para o kernel. */
#include "mm/pmm.h"
#include "lib/string.h"

#define PMM_START  0x200000UL
#define PMM_END    0x4000000UL
#define NFRAMES    ((PMM_END - PMM_START) / PAGE_SIZE)

static uint8_t bitmap[NFRAMES / 8];
static size_t used;
static size_t total = NFRAMES;

static bool frame_used(size_t i)  { return bitmap[i / 8] & (1 << (i % 8)); }
static void frame_set(size_t i)   { bitmap[i / 8] |= (uint8_t)(1 << (i % 8)); }
static void frame_clear(size_t i) { bitmap[i / 8] &= (uint8_t)~(1 << (i % 8)); }

/* Boot por BIOS: 2-64 MiB e memoria livre em qualquer PC. */
void pmm_init(void)
{
    memset(bitmap, 0, sizeof bitmap);
    used = 0;
    total = NFRAMES;
}

/* Boot por UEFI: so as faixas que o firmware informou como livres. O que
 * estiver fora delas (firmware, tabelas ACPI, o proprio kernel, os
 * programas carregados) fica marcado como ocupado para sempre. */
void pmm_init_ranges(const struct uefi_mem_range *ranges, size_t count)
{
    memset(bitmap, 0xFF, sizeof bitmap);
    used = 0;
    total = 0;
    for (size_t r = 0; r < count; r++) {
        uint64_t start = ALIGN_UP(ranges[r].start, PAGE_SIZE);
        uint64_t end = ranges[r].start + ranges[r].pages * PAGE_SIZE;
        if (start < PMM_START)
            start = PMM_START;
        if (end > PMM_END)
            end = PMM_END;
        for (uint64_t a = start; a + PAGE_SIZE <= end; a += PAGE_SIZE) {
            size_t i = (a - PMM_START) / PAGE_SIZE;
            if (frame_used(i)) {
                frame_clear(i);
                total++;
            }
        }
    }
}

uint64_t pmm_alloc_pages(size_t n)
{
    size_t run = 0;
    for (size_t i = 0; i < NFRAMES; i++) {
        if (frame_used(i)) {
            run = 0;
            continue;
        }
        if (++run == n) {
            size_t first = i + 1 - n;
            for (size_t j = first; j <= i; j++)
                frame_set(j);
            used += n;
            uint64_t addr = PMM_START + first * PAGE_SIZE;
            memset((void *)addr, 0, n * PAGE_SIZE);
            return addr;
        }
    }
    return 0;
}

void pmm_free_pages(uint64_t addr, size_t n)
{
    if (addr < PMM_START || addr >= PMM_END)
        return;
    size_t first = (addr - PMM_START) / PAGE_SIZE;
    for (size_t j = first; j < first + n && j < NFRAMES; j++) {
        if (frame_used(j)) {
            frame_clear(j);
            used--;
        }
    }
}

uint64_t pmm_alloc_page(void)         { return pmm_alloc_pages(1); }
void pmm_free_page(uint64_t addr)     { pmm_free_pages(addr, 1); }
size_t pmm_used_pages(void)           { return used; }
size_t pmm_total_pages(void)          { return total; }
