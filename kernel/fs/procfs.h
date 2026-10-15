#ifndef FS_PROCFS_H
#define FS_PROCFS_H

#include "types.h"

/* PROCFS: sistema de arquivos minimo, criado por tools/mkdisk.py.
 * No setor FS_LBA fica um diretorio: cabecalho de 64 bytes + ate 63
 * entradas de 64 bytes (nome, setor inicial, tamanho, flags). */
#define FS_LBA           2048
#define FS_DIR_SECTORS   8
#define FS_MAX_FILES     63
#define FS_FLAG_AUTORUN  1      /* executar automaticamente no boot */

struct fs_entry {
    char name[40];
    uint32_t lba;
    uint32_t size;
    uint32_t flags;
    uint8_t reserved[12];
} __attribute__((packed));

bool fs_init(void);
int fs_count(void);
const struct fs_entry *fs_entry_at(int i);
const struct fs_entry *fs_find(const char *name);
bool fs_read(const struct fs_entry *f, void *buf);         /* buf >= setores * 512 */
bool fs_read_head(const struct fs_entry *f, void *sector); /* so o 1o setor */

#endif
