#ifndef BOOT_H
#define BOOT_H

#include "types.h"

/* Recado que o setor de boot (boot/boot.asm) deixa em 0x600 para o kernel. */
#define BOOTINFO_ADDR  0x600
#define BOOTINFO_MAGIC 0x544F4250       /* "PBOT" */

struct bootinfo {
    uint32_t magic;
    uint32_t ramdisk_sectors;   /* setores do PROCFS que a BIOS copiou para a RAM */
    uint32_t ramdisk_addr;      /* onde esta a copia (0x30000) */
    uint8_t  boot_drive;        /* numero do disco na BIOS: 0x80 = HD/pendrive */
} __attribute__((packed));

static inline const struct bootinfo *bootinfo(void)
{
    const struct bootinfo *b;
    /* o "asm" so esconde do gcc que o ponteiro vem de um endereco fixo
     * baixo (senao ele avisa como se fosse um ponteiro nulo) */
    __asm__ ("" : "=r"(b) : "0"((uintptr_t)BOOTINFO_ADDR));
    return b->magic == BOOTINFO_MAGIC ? b : NULL;
}

#endif
