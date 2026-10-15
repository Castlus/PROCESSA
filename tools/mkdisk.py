#!/usr/bin/env python3
"""
mkdisk.py - monta a imagem de disco do PROCESSA.

Layout da imagem (setores de 512 bytes):
  LBA 0             setor de boot (boot/boot.asm)
  LBA 1 .. 2047     kernel (o boot carrega os primeiros 256 setores = 128 KiB)
  LBA 2048 .. 2055  diretorio do PROCFS: cabecalho + ate 63 entradas
  LBA 2056 ..       conteudo dos arquivos, cada um comecando num setor novo

Uso:
  mkdisk.py saida.img boot.bin kernel.bin [--auto] arquivo [[--auto] arquivo ...]

  --auto  marca o arquivo seguinte para ser executado automaticamente no boot.
"""
import os
import struct
import sys

SECTOR = 512
KERNEL_LBA = 1
KERNEL_MAX_SECTORS = 256
FS_LBA = 2048
DIR_SECTORS = 8
ENTRY_SIZE = 64
MAX_FILES = DIR_SECTORS * SECTOR // ENTRY_SIZE - 1
FLAG_AUTORUN = 1


def die(msg):
    print(f"mkdisk: {msg}", file=sys.stderr)
    sys.exit(1)


def read(path):
    try:
        with open(path, "rb") as f:
            return f.read()
    except OSError as e:
        die(f"nao consegui ler {path}: {e}")


def main(argv):
    if len(argv) < 4:
        die(__doc__)
    out, boot_path, kernel_path = argv[1:4]

    files, auto = [], False
    for arg in argv[4:]:
        if arg == "--auto":
            auto = True
            continue
        files.append((arg, auto))
        auto = False

    boot = read(boot_path)
    if len(boot) != SECTOR or boot[510:] != b"\x55\xaa":
        die("setor de boot invalido (precisa ter 512 bytes e terminar em 55 AA)")

    kernel = read(kernel_path)
    if len(kernel) > KERNEL_MAX_SECTORS * SECTOR:
        die(f"kernel tem {len(kernel)} bytes; o boot so carrega {KERNEL_MAX_SECTORS * SECTOR}")
    if len(files) > MAX_FILES:
        die(f"no maximo {MAX_FILES} arquivos")

    image = bytearray(FS_LBA * SECTOR)
    image[0:SECTOR] = boot
    image[KERNEL_LBA * SECTOR:KERNEL_LBA * SECTOR + len(kernel)] = kernel

    directory = bytearray(DIR_SECTORS * SECTOR)
    data = bytearray()
    lba = FS_LBA + DIR_SECTORS
    names = set()
    for i, (path, autorun) in enumerate(files):
        content = read(path)
        name = os.path.basename(path).encode()
        if len(name) > 39:
            die(f"nome muito longo (max. 39 caracteres): {path}")
        if name in names:
            die(f"dois arquivos com o mesmo nome no disco: {name.decode()}")
        names.add(name)
        flags = FLAG_AUTORUN if autorun else 0
        # nome[40], lba, tamanho, flags, reservado[12]
        struct.pack_into("<40sIII12x", directory, ENTRY_SIZE * (i + 1), name, lba, len(content), flags)
        padded = content + b"\0" * (-len(content) % SECTOR)
        data += padded
        lba += len(padded) // SECTOR
        print(f"  {name.decode():<16} {len(content):>8} bytes  LBA {lba - len(padded) // SECTOR}"
              f"{'  [boot]' if autorun else ''}")

    struct.pack_into("<8sI52x", directory, 0, b"PROCFS01", len(files))
    image += directory + data
    image += b"\0" * (-len(image) % (1024 * 1024))   # arredonda para 1 MiB

    with open(out, "wb") as f:
        f.write(image)
    print(f"  -> {out}: {len(image) // 1024} KiB, kernel {len(kernel)} bytes, {len(files)} arquivo(s)")


if __name__ == "__main__":
    main(sys.argv)
