/* PROCFS: le o diretorio e os arquivos de uma de duas fontes:
 *   - o disco ATA, pelo driver do kernel (QEMU, PCs com IDE);
 *   - a copia que o setor de boot pediu a BIOS para fazer na RAM. Num
 *     notebook bootando por pendrive USB nao existe disco ATA, e escrever
 *     um driver USB do zero e outro projeto; a BIOS ja sabe ler o pendrive. */
#include "fs/procfs.h"
#include "boot.h"
#include "drivers/ata.h"
#include "lib/string.h"

struct fs_header {
    char magic[8];              /* "PROCFS01" */
    uint32_t count;
    uint8_t reserved[52];
} __attribute__((packed));

static enum { SRC_NONE, SRC_ATA, SRC_RAM } source;
static const uint8_t *ram;
static uint32_t ram_sectors;

static uint8_t directory[FS_DIR_SECTORS * SECTOR_SIZE];
static struct fs_entry *entries;
static int count;

static bool disk_read(uint32_t lba, uint32_t n, void *buf)
{
    if (source == SRC_ATA)
        return ata_read(lba, n, buf);
    if (source != SRC_RAM || lba < FS_LBA)
        return false;
    uint32_t rel = lba - FS_LBA;
    if (rel > ram_sectors || n > ram_sectors - rel)
        return false;               /* alem do que a BIOS copiou */
    memcpy(buf, ram + (uint64_t)rel * SECTOR_SIZE, (size_t)n * SECTOR_SIZE);
    return true;
}

static bool mount(void)
{
    if (!disk_read(FS_LBA, FS_DIR_SECTORS, directory))
        return false;

    struct fs_header *h = (struct fs_header *)directory;
    if (memcmp(h->magic, "PROCFS01", 8) != 0)
        return false;

    count = h->count > FS_MAX_FILES ? FS_MAX_FILES : (int)h->count;
    entries = (struct fs_entry *)(directory + sizeof *h);
    for (int i = 0; i < count; i++)
        entries[i].name[sizeof entries[i].name - 1] = '\0';
    return true;
}

bool fs_init(bool have_ata)
{
    if (have_ata) {
        source = SRC_ATA;
        if (mount())
            return true;
    }

    const struct bootinfo *b = bootinfo();
    if (b && b->ramdisk_sectors >= FS_DIR_SECTORS) {
        source = SRC_RAM;
        ram = (const uint8_t *)(uintptr_t)b->ramdisk_addr;
        ram_sectors = b->ramdisk_sectors;
        if (mount())
            return true;
    }

    source = SRC_NONE;
    return false;
}

const char *fs_source(void)
{
    switch (source) {
    case SRC_ATA: return "disco ATA, pelo driver do kernel";
    case SRC_RAM: return "cópia do disco feita pela BIOS no boot";
    default:      return "nenhum";
    }
}

int fs_count(void)
{
    return count;
}

const struct fs_entry *fs_entry_at(int i)
{
    return (i >= 0 && i < count) ? &entries[i] : NULL;
}

const struct fs_entry *fs_find(const char *name)
{
    for (int i = 0; i < count; i++)
        if (strcmp(entries[i].name, name) == 0)
            return &entries[i];
    return NULL;
}

bool fs_read(const struct fs_entry *f, void *buf)
{
    uint32_t sectors = (f->size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    return disk_read(f->lba, sectors, buf);
}

bool fs_read_head(const struct fs_entry *f, void *sector)
{
    return disk_read(f->lba, 1, sector);
}
