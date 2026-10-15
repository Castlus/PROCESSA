/* =====================================================================
 *  LOADER: do arquivo no disco ate um processo pronto para rodar.
 *    1. le o arquivo inteiro do disco (driver ATA + PROCFS)
 *    2. descobre o formato pelos primeiros bytes (nao pela extensao!)
 *    3. cria um espaco de enderecamento novo (tabela de paginas propria)
 *    4. o formato copia os segmentos para la e informa o ponto de entrada
 *    5. cria o PCB, que entra na fila de PRONTOS do escalonador
 * ===================================================================== */
#include "loader/loader.h"
#include "drivers/console.h"
#include "fs/procfs.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "lib/printf.h"
#include "lib/stdarg.h"

#define LOADER_MAX_FILE (4 * 1024 * 1024)

/* Ordem importa: o binario puro aceita qualquer coisa, entao vem por ultimo. */
static const struct binfmt *const formats[] = {
    &binfmt_elf64,
    &binfmt_pe64,
    &binfmt_macho64,
    &binfmt_raw,
};

bool loader_fail(const char *fmt, ...)
{
    char msg[160];
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    kprintf_color(COLOR_ERROR, "[loader] erro: %s\n", msg);
    return false;
}

const struct binfmt *loader_detect(const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < sizeof formats / sizeof formats[0]; i++)
        if (formats[i]->probe(data, size))
            return formats[i];
    return &binfmt_raw;
}

bool loader_map_segment(uint64_t cr3, uint64_t vaddr, uint64_t memsz,
                        const uint8_t *src, uint64_t filesz)
{
    if (memsz == 0)
        return true;
    if (filesz > memsz)
        return loader_fail("segmento com mais dados no arquivo que na memória");
    if (vaddr < USER_BASE || vaddr >= USER_IMAGE_TOP || memsz > USER_IMAGE_TOP - vaddr)
        return loader_fail("segmento em %p fora da área de usuário (0x40000000-0x7FFF0000)",
                           (void *)vaddr);
    if (!vmm_alloc_range(cr3, vaddr, memsz))
        return loader_fail("sem memória para o segmento em %p", (void *)vaddr);
    /* paginas novas ja vem zeradas: o resto (memsz - filesz, ex. .bss) fica 0 */
    if (filesz && !vmm_copy_to(cr3, vaddr, src, filesz))
        return loader_fail("falha copiando segmento");
    return true;
}

struct process *loader_exec(const char *filename)
{
    const struct fs_entry *f = fs_find(filename);
    if (!f) {
        loader_fail("arquivo '%s' não existe no disco (use 'ls')", filename);
        return NULL;
    }
    if (f->size == 0 || f->size > LOADER_MAX_FILE) {
        loader_fail("'%s' tem tamanho inválido (%u bytes)", filename, f->size);
        return NULL;
    }

    size_t npages = (f->size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint8_t *data = (uint8_t *)pmm_alloc_pages(npages);
    if (!data) {
        loader_fail("sem memória para ler '%s'", filename);
        return NULL;
    }
    if (!fs_read(f, data)) {
        pmm_free_pages((uint64_t)data, npages);
        loader_fail("erro de leitura do disco em '%s'", filename);
        return NULL;
    }

    const struct binfmt *fmt = loader_detect(data, f->size);
    kprintf("[loader] %s: %u bytes, formato detectado: %s (%s)\n",
            f->name, f->size, fmt->name, fmt->origin);

    struct process *p = NULL;
    uint64_t entry = 0;
    uint64_t cr3 = vmm_create_space();
    if (!cr3) {
        loader_fail("sem memória para a tabela de páginas");
    } else if (!fmt->load(data, f->size, cr3, &entry)) {
        /* o formato ja explicou o erro */
    } else if (entry < USER_BASE || entry >= USER_IMAGE_TOP || !vmm_translate(cr3, entry)) {
        loader_fail("ponto de entrada %p não está no programa carregado", (void *)entry);
    } else if (!(p = proc_create(f->name, fmt->name, cr3, entry))) {
        loader_fail("tabela de processos cheia ou sem memória");
    }

    pmm_free_pages((uint64_t)data, npages);
    if (!p) {
        vmm_destroy_space(cr3);
        return NULL;
    }
    kprintf("[loader] %s -> PID %d, entrada %p, %lu KiB, CR3 %p\n",
            p->name, p->pid, (void *)p->entry, p->pages * 4, (void *)p->cr3);
    return p;
}
