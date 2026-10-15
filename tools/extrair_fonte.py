#!/usr/bin/env python3
"""
extrair_fonte.py - gera kernel/drivers/font8x16.c a partir de uma ROM de VGA.

No boot UEFI nao existe o modo texto da VGA: o kernel desenha cada caractere
pixel a pixel na tela grafica, e para isso precisa da "fonte" (o desenho de
cada caractere). Usamos a mesma fonte 8x16 do modo texto VGA (tabela CP437),
copiada da VGA BIOS que vem com o QEMU (SeaBIOS), para a tela ficar igual nos
dois modos de boot.

Uso: extrair_fonte.py /usr/share/seabios/vgabios-stdvga.bin kernel/drivers/font8x16.c
"""
import sys

GLYPH_A = bytes([0x00, 0x00, 0x10, 0x38, 0x6C, 0xC6, 0xC6, 0xFE,
                 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00])
GLYPH_SMILE = bytes([0x00, 0x00, 0x7E, 0x81, 0xA5, 0x81, 0x81, 0xBD,
                     0x99, 0x81, 0x81, 0x7E, 0x00, 0x00, 0x00, 0x00])


def main(rom_path, out_path):
    rom = open(rom_path, "rb").read()
    pos = rom.find(GLYPH_A)
    while pos >= 0:
        start = pos - 0x41 * 16
        if start >= 0 and rom[start + 16:start + 32] == GLYPH_SMILE:
            break
        pos = rom.find(GLYPH_A, pos + 1)
    else:
        sys.exit("fonte 8x16 nao encontrada na ROM")

    font = rom[start:start + 256 * 16]
    lines = []
    for ch in range(256):
        glyph = ", ".join(f"0x{b:02x}" for b in font[ch * 16:(ch + 1) * 16])
        label = chr(ch) if 0x20 <= ch < 0x7F else ""
        lines.append(f"    {glyph}, /* 0x{ch:02x} {label} */")

    with open(out_path, "w") as f:
        f.write("/* Fonte 8x16 do modo texto VGA (tabela CP437): 256 caracteres, 16 bytes\n"
                " * cada, um byte por linha, bit 7 = pixel da esquerda.\n"
                " * Gerado por tools/extrair_fonte.py a partir da VGA BIOS do QEMU (SeaBIOS).\n"
                " * Usado so no boot UEFI, onde o kernel desenha o texto pixel a pixel. */\n"
                "#include \"types.h\"\n\n"
                "const uint8_t font8x16[256 * 16] = {\n")
        f.write("\n".join(lines))
        f.write("\n};\n")
    print(f"fonte extraida do offset 0x{start:x} -> {out_path}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
