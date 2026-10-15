/* Layout do kernel na memoria. Quem carrega a imagem "crua" deste ELF
 * pula para o primeiro byte, entao _start (.text.entry) vem primeiro.
 *   boot por BIOS: kernel.bin em 0x8000 (padrao)
 *   boot por UEFI: kernel-uefi.bin em 0x100000 (--defsym=KERNEL_BASE=...) */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = DEFINED(KERNEL_BASE) ? KERNEL_BASE : 0x8000;
    __kernel_start = .;

    .text : {
        *(.text.entry)
        *(.text .text.*)
    }

    .rodata : {
        *(.rodata .rodata.*)
    }

    .data : {
        *(.data .data.*)
        *(.got .got.*)
    }

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note*)
        *(.comment*)
    }
}

/* BIOS: o kernel nao pode encostar na copia do PROCFS em 0x30000.
 * UEFI: o carregador reserva 1 MiB para o kernel. */
ASSERT(__kernel_start >= 0x100000 || __bss_end <= 0x30000, "kernel BIOS passou de 0x30000")
ASSERT(__bss_end - __kernel_start <= 0x100000, "kernel passou de 1 MiB")
