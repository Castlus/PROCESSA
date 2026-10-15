#ifndef BOOT_H
#define BOOT_H

#include "types.h"
#include "bootinfo.h"       /* boot/bootinfo.h: o que o carregador UEFI entrega */

/* Como a maquina deu boot. O kernel aceita dois caminhos:
 *   BIOS: boot/boot.asm deixa um recado em 0x600 (struct bios_bootinfo);
 *   UEFI: boot/uefi.c passa um ponteiro em RDI (struct uefi_bootinfo). */
enum boot_kind {
    BOOT_BIOS,
    BOOT_UEFI,
};

struct boot_params {
    enum boot_kind kind;
    const uint8_t *ramdisk;         /* copia do PROCFS na memoria (ou NULL) */
    uint32_t ramdisk_sectors;
    uint8_t bios_drive;             /* BIOS: numero do disco de boot */
    const struct uefi_bootinfo *uefi;   /* UEFI: tela, mapa de memoria, TSC */
};

extern struct boot_params boot;

void boot_parse(uint64_t arg);

#endif
