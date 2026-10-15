/* PE32+ (Portable Executable 64 bits) - formato .exe do Windows.
 *   [cabecalho MS-DOS "MZ"] -> e_lfanew aponta para a assinatura "PE\0\0"
 *   [COFF header] maquina, numero de secoes
 *   [Optional header] ImageBase, ponto de entrada (RVA), diretorios
 *   [Tabela de secoes] .text, .data... cada uma com RVA e offset no arquivo
 * RVA = endereco relativo a ImageBase. */
#include "loader/loader.h"

#define PE_MACHINE_AMD64 0x8664
#define PE_MACHINE_I386  0x014C
#define PE32PLUS_MAGIC   0x020B

static bool pe_probe(const uint8_t *d, size_t n)
{
    return n >= 2 && d[0] == 'M' && d[1] == 'Z';
}

static bool pe_load(const uint8_t *d, size_t n, uint64_t cr3, uint64_t *entry)
{
    if (n < 0x40)
        return loader_fail("PE truncado");
    uint32_t lfanew = rd32(d + 0x3C);
    if (lfanew > n || n - lfanew < 24)
        return loader_fail("cabeçalho PE fora do arquivo");

    const uint8_t *pe = d + lfanew;
    if (pe[0] != 'P' || pe[1] != 'E' || pe[2] || pe[3])
        return loader_fail("sem assinatura PE (é um executável MS-DOS?)");

    uint16_t machine = rd16(pe + 4);
    uint16_t nsections = rd16(pe + 6);
    uint16_t optsize = rd16(pe + 20);
    const uint8_t *opt = pe + 24;

    if (machine == PE_MACHINE_I386)
        return loader_fail("PE de 32 bits (i386): esta máquina só executa 64 bits");
    if (machine != PE_MACHINE_AMD64)
        return loader_fail("PE para outra arquitetura (machine=0x%x)", machine);
    if (optsize < 112 || (uint64_t)(opt - d) + optsize > n)
        return loader_fail("optional header inválido");
    if (rd16(opt) != PE32PLUS_MAGIC)
        return loader_fail("não é PE32+ (64 bits)");

    uint32_t entry_rva = rd32(opt + 16);
    uint64_t image_base = rd64(opt + 24);
    uint32_t headers_size = rd32(opt + 60);
    uint32_t ndirs = rd32(opt + 108);

    /* diretorio 1 = tabela de importacao (DLLs) */
    if (ndirs > 1 && optsize >= 112 + 2 * 8 && rd32(opt + 112 + 8 + 4) != 0)
        return loader_fail("o programa importa DLLs, mas aqui não existe Windows");

    const uint8_t *sections = opt + optsize;
    if ((uint64_t)(sections - d) + (uint64_t)nsections * 40 > n)
        return loader_fail("tabela de seções inválida");

    /* como no Windows, os cabecalhos tambem ficam mapeados em ImageBase */
    if (headers_size > n)
        headers_size = (uint32_t)n;
    if (!loader_map_segment(cr3, image_base, headers_size, d, headers_size))
        return false;

    for (int i = 0; i < nsections; i++) {
        const uint8_t *s = sections + i * 40;
        uint32_t vsize = rd32(s + 8);
        uint32_t rva = rd32(s + 12);
        uint32_t raw_size = rd32(s + 16);
        uint32_t raw_ptr = rd32(s + 20);
        uint64_t memsz = vsize ? vsize : raw_size;
        uint64_t filesz = raw_size < memsz ? raw_size : memsz;

        if (filesz && (raw_ptr > n || filesz > n - raw_ptr))
            return loader_fail("seção %.8s passa do fim do arquivo", (const char *)s);
        if (!loader_map_segment(cr3, image_base + rva, memsz, d + raw_ptr, filesz))
            return false;
    }

    *entry = image_base + entry_rva;
    return true;
}

const struct binfmt binfmt_pe64 = { "PE32+", "Windows", pe_probe, pe_load };
