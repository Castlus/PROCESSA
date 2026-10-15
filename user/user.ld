/* Layout dos programas de usuario: todos comecam em 0x40000000 (cabecalhos)
 * e o codigo em 0x40001000. Como cada processo tem a sua tabela de paginas,
 * varios programas usam os MESMOS enderecos sem conflito. */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x40001000;
    .text : { *(.text .text.*) }

    . = ALIGN(0x1000);
    .rodata : { *(.rodata .rodata.*) }

    . = ALIGN(0x1000);
    .data : { *(.data .data.*) *(.got .got.*) }
    .bss : { *(.bss .bss.*) *(COMMON) }

    /DISCARD/ : { *(.eh_frame*) *(.note*) *(.comment*) }
}
