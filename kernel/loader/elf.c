/* ELF64 - formato executavel do Linux e da maioria dos Unix.
 *   [cabecalho ELF] -> aponta para a tabela de "program headers"
 *   cada program header PT_LOAD diz: copie N bytes do arquivo (offset)
 *   para o endereco virtual V, ocupando M bytes na memoria. */
#include "loader/loader.h"

#define ET_EXEC     2
#define ET_DYN      3
#define EM_X86_64   62
#define PT_LOAD     1

struct elf64_ehdr {
    uint8_t  ident[16];         /* 0x7F 'E' 'L' 'F', classe, endianness... */
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed));

struct elf64_phdr {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed));

static bool elf_probe(const uint8_t *d, size_t n)
{
    return n >= 4 && d[0] == 0x7F && d[1] == 'E' && d[2] == 'L' && d[3] == 'F';
}

static bool elf_load(const uint8_t *d, size_t n, uint64_t cr3, uint64_t *entry)
{
    if (n < sizeof(struct elf64_ehdr))
        return loader_fail("ELF truncado");

    const struct elf64_ehdr *h = (const struct elf64_ehdr *)d;
    if (h->ident[4] != 2)
        return loader_fail("ELF de 32 bits: esta máquina só executa 64 bits");
    if (h->ident[5] != 1)
        return loader_fail("ELF big-endian não suportado");
    if (h->machine != EM_X86_64)
        return loader_fail("ELF para outra arquitetura (machine=%u), não x86-64", h->machine);
    if (h->type == ET_DYN)
        return loader_fail("ELF dinâmico/PIE (ET_DYN): depende do carregador e das "
                           "bibliotecas do Linux; aqui só ELF estático (ET_EXEC)");
    if (h->type != ET_EXEC)
        return loader_fail("ELF não é executável (type=%u)", h->type);
    if (h->phentsize != sizeof(struct elf64_phdr) || h->phoff > n ||
        (uint64_t)h->phnum * sizeof(struct elf64_phdr) > n - h->phoff)
        return loader_fail("tabela de program headers inválida");

    int loaded = 0;
    for (int i = 0; i < h->phnum; i++) {
        const struct elf64_phdr *ph =
            (const struct elf64_phdr *)(d + h->phoff + i * sizeof(struct elf64_phdr));
        if (ph->type != PT_LOAD)
            continue;
        if (ph->offset > n || ph->filesz > n - ph->offset)
            return loader_fail("segmento passa do fim do arquivo");
        if (!loader_map_segment(cr3, ph->vaddr, ph->memsz, d + ph->offset, ph->filesz))
            return false;
        loaded++;
    }
    if (!loaded)
        return loader_fail("ELF sem segmentos PT_LOAD");

    *entry = h->entry;
    return true;
}

const struct binfmt binfmt_elf64 = { "ELF64", "Linux/Unix", elf_probe, elf_load };
