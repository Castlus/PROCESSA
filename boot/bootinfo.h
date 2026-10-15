/* Informacoes que o carregador UEFI (boot/uefi.c) entrega ao kernel em RDI.
 * Este cabecalho e usado pelos dois lados (o carregador e compilado com o
 * mingw, onde "long" tem 32 bits), entao so usa tipos de tamanho fixo. */
#ifndef BOOT_BOOTINFO_H
#define BOOT_BOOTINFO_H

#define UEFI_BOOTINFO_MAGIC 0x49464555u     /* "UEFI" */
#define UEFI_MEM_MAX        128
#define UEFI_KERNEL_ADDR    0x100000ull     /* kernel-uefi.bin e ligado aqui */

/* faixa de memoria fisica livre (em paginas de 4 KiB) */
struct uefi_mem_range {
    unsigned long long start;
    unsigned long long pages;
};

struct uefi_bootinfo {
    unsigned int magic;
    unsigned int mem_count;
    unsigned long long ramdisk_addr;        /* PROCFS montado pelo carregador */
    unsigned long long ramdisk_sectors;
    unsigned long long fb_addr;             /* tela grafica (GOP) */
    unsigned int fb_width, fb_height;
    unsigned int fb_stride;                 /* pixels por linha */
    unsigned int fb_format;                 /* 0 = RGB, 1 = BGR, 2 = mascaras */
    unsigned int mask_r, mask_g, mask_b, reserved;
    unsigned long long tsc_hz;              /* frequencia do TSC, medida com Stall() */
    char firmware[48];                      /* fabricante do firmware */
    struct uefi_mem_range mem[UEFI_MEM_MAX];
};

#endif
