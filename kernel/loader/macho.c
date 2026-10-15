/* Mach-O 64 - formato executavel do macOS.
 *   [mach_header_64] magic 0xFEEDFACF, tipo de CPU, numero de comandos
 *   [load commands]  LC_SEGMENT_64: mapeie "filesize" bytes do offset
 *                    "fileoff" no endereco "vmaddr" (tamanho "vmsize")
 *                    LC_UNIXTHREAD: registradores iniciais (rip = entrada)
 *                    LC_MAIN: entrada como offset dentro do __TEXT */
#include "loader/loader.h"

#define MH_MAGIC_64      0xFEEDFACF
#define MH_MAGIC         0xFEEDFACE
#define CPU_TYPE_X86_64  0x01000007
#define CPU_TYPE_ARM64   0x0100000C
#define MH_EXECUTE       2
#define LC_SEGMENT_64    0x19
#define LC_UNIXTHREAD    0x5
#define LC_MAIN          0x80000028
#define X86_THREAD_STATE64 4

static bool macho_probe(const uint8_t *d, size_t n)
{
    if (n < 4)
        return false;
    uint32_t magic = rd32(d);
    return magic == MH_MAGIC_64 || magic == MH_MAGIC;
}

static bool macho_load(const uint8_t *d, size_t n, uint64_t cr3, uint64_t *entry)
{
    if (n < 32)
        return loader_fail("Mach-O truncado");
    if (rd32(d) == MH_MAGIC)
        return loader_fail("Mach-O de 32 bits: esta máquina só executa 64 bits");

    uint32_t cputype = rd32(d + 4);
    if (cputype == CPU_TYPE_ARM64)
        return loader_fail("Mach-O para ARM64 (Apple Silicon); esta CPU é x86-64");
    if (cputype != CPU_TYPE_X86_64)
        return loader_fail("Mach-O para outra CPU (cputype=0x%x)", cputype);
    if (rd32(d + 12) != MH_EXECUTE)
        return loader_fail("Mach-O não é executável (MH_EXECUTE)");

    uint32_t ncmds = rd32(d + 16);
    uint64_t cmds_end = 32 + (uint64_t)rd32(d + 20);
    if (cmds_end > n)
        return loader_fail("load commands passam do fim do arquivo");

    uint64_t off = 32, text_vmaddr = 0, thread_rip = 0, main_off = 0;
    bool have_thread = false, have_main = false;
    int loaded = 0;

    for (uint32_t i = 0; i < ncmds; i++) {
        if (off + 8 > cmds_end)
            return loader_fail("load command truncado");
        const uint8_t *c = d + off;
        uint32_t cmd = rd32(c), cmdsize = rd32(c + 4);
        if (cmdsize < 8 || off + cmdsize > cmds_end)
            return loader_fail("load command com tamanho inválido");

        if (cmd == LC_SEGMENT_64 && cmdsize >= 72) {
            uint64_t vmaddr = rd64(c + 24), vmsize = rd64(c + 32);
            uint64_t fileoff = rd64(c + 40), filesize = rd64(c + 48);
            uint32_t initprot = rd32(c + 60);
            if (fileoff == 0 && filesize)
                text_vmaddr = vmaddr;           /* __TEXT comeca no inicio do arquivo */
            if (vmsize && initprot) {           /* pula __PAGEZERO (sem permissao) */
                if (fileoff > n || filesize > n - fileoff)
                    return loader_fail("segmento passa do fim do arquivo");
                if (filesize > vmsize)
                    filesize = vmsize;
                if (!loader_map_segment(cr3, vmaddr, vmsize, d + fileoff, filesize))
                    return false;
                loaded++;
            }
        } else if (cmd == LC_UNIXTHREAD && cmdsize >= 16 + 17 * 8) {
            if (rd32(c + 8) == X86_THREAD_STATE64) {
                thread_rip = rd64(c + 16 + 16 * 8);  /* rax..r15 (16 regs), depois rip */
                have_thread = true;
            }
        } else if (cmd == LC_MAIN && cmdsize >= 16) {
            main_off = rd64(c + 8);
            have_main = true;
        }
        off += cmdsize;
    }

    if (!loaded)
        return loader_fail("Mach-O sem segmentos");
    if (have_main)
        *entry = text_vmaddr + main_off;
    else if (have_thread)
        *entry = thread_rip;
    else
        return loader_fail("Mach-O sem ponto de entrada (LC_MAIN/LC_UNIXTHREAD)");
    return true;
}

const struct binfmt binfmt_macho64 = { "Mach-O", "macOS", macho_probe, macho_load };
