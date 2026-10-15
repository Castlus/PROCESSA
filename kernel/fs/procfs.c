#include "fs/procfs.h"
#include "drivers/ata.h"
#include "lib/string.h"

struct fs_header {
    char magic[8];              /* "PROCFS01" */
    uint32_t count;
    uint8_t reserved[52];
} __attribute__((packed));

static uint8_t directory[FS_DIR_SECTORS * SECTOR_SIZE];
static struct fs_entry *entries;
static int count;

bool fs_init(void)
{
    if (!ata_read(FS_LBA, FS_DIR_SECTORS, directory))
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
    return ata_read(f->lba, sectors, buf);
}

bool fs_read_head(const struct fs_entry *f, void *sector)
{
    return ata_read(f->lba, 1, sector);
}
