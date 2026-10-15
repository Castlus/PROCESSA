#ifndef LOADER_LOADER_H
#define LOADER_LOADER_H

#include "types.h"
#include "proc/process.h"

/* Cada formato de executavel e um "plugin" com duas funcoes:
 *   probe: olha os primeiros bytes (numero magico) e diz se e dele;
 *   load:  entende o cabecalho e copia os segmentos para a memoria do
 *          processo, devolvendo o ponto de entrada.
 * Suportar um formato novo = escrever mais um desses e por na lista. */
struct binfmt {
    const char *name;           /* aparece na tabela de processos */
    const char *origin;
    bool (*probe)(const uint8_t *data, size_t size);
    bool (*load)(const uint8_t *data, size_t size, uint64_t cr3, uint64_t *entry);
};

extern const struct binfmt binfmt_elf64;
extern const struct binfmt binfmt_pe64;
extern const struct binfmt binfmt_macho64;
extern const struct binfmt binfmt_raw;

const struct binfmt *loader_detect(const uint8_t *data, size_t size);
struct process *loader_exec(const char *filename);

/* utilitarios para os formatos */
bool loader_map_segment(uint64_t cr3, uint64_t vaddr, uint64_t memsz,
                        const uint8_t *src, uint64_t filesz);
bool loader_fail(const char *fmt, ...);

static inline uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint64_t rd64(const uint8_t *p)
{
    return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}

#endif
