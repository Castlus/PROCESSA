/* Descobre por qual caminho a maquina deu boot e o que o carregador deixou. */
#include "boot.h"

/* Recado do setor de boot da BIOS (boot/boot.asm) em 0x600. */
#define BIOS_BOOTINFO_ADDR  0x600
#define BIOS_BOOTINFO_MAGIC 0x544F4250      /* "PBOT" */

struct bios_bootinfo {
    uint32_t magic;
    uint32_t ramdisk_sectors;
    uint32_t ramdisk_addr;
    uint8_t  boot_drive;
} __attribute__((packed));

struct boot_params boot;

/* O "asm" so esconde do gcc que o ponteiro vem de um endereco fixo baixo
 * (senao ele avisa como se fosse um ponteiro nulo). */
static const void *fixed_address(uint64_t addr)
{
    const void *p;
    __asm__ ("" : "=r"(p) : "0"(addr));
    return p;
}

void boot_parse(uint64_t arg)
{
    /* UEFI: RDI aponta para as informacoes (sempre abaixo de 1 GiB). No boot
     * por BIOS, RDI tem lixo do modo real: so confiamos se tiver a assinatura. */
    if (arg && arg < 0x40000000UL && !(arg & 7)) {
        const struct uefi_bootinfo *u = fixed_address(arg);
        if (u->magic == UEFI_BOOTINFO_MAGIC) {
            boot.kind = BOOT_UEFI;
            boot.uefi = u;
            boot.ramdisk = (const uint8_t *)u->ramdisk_addr;
            boot.ramdisk_sectors = (uint32_t)u->ramdisk_sectors;
            return;
        }
    }

    boot.kind = BOOT_BIOS;
    const struct bios_bootinfo *b = fixed_address(BIOS_BOOTINFO_ADDR);
    if (b->magic == BIOS_BOOTINFO_MAGIC) {
        boot.ramdisk = (const uint8_t *)(uintptr_t)b->ramdisk_addr;
        boot.ramdisk_sectors = b->ramdisk_sectors;
        boot.bios_drive = b->boot_drive;
    }
}
