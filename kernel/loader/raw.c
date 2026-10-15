/* Binario puro ("flat binary"): sem cabecalho nenhum, so codigo de
 * maquina. E o formato universal de ultimo recurso: o arquivo inteiro e
 * copiado para 0x40000000 e a execucao comeca no primeiro byte. */
#include "loader/loader.h"
#include "mm/vmm.h"

static bool raw_probe(const uint8_t *d, size_t n)
{
    (void)d;
    (void)n;
    return true;
}

static bool raw_load(const uint8_t *d, size_t n, uint64_t cr3, uint64_t *entry)
{
    if (!loader_map_segment(cr3, USER_BASE, n, d, n))
        return false;
    *entry = USER_BASE;
    return true;
}

const struct binfmt binfmt_raw = { "RAW", "binário puro", raw_probe, raw_load };
