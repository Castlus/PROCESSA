/* Layout do kernel na memoria. O setor de boot carrega o arquivo
 * kernel.bin (imagem "crua" deste ELF) em 0x8000 e pula para o inicio,
 * entao _start (secao .text.entry) precisa ser o primeiro byte. */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x8000;

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
