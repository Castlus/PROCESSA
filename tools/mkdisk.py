#!/usr/bin/env python3
"""
mkdisk.py - monta a imagem de disco do PROCESSA.

Layout da imagem (setores de 512 bytes):
  LBA 0             setor de boot (boot/boot.asm)
  LBA 1 .. 2047     kernel (o boot carrega os primeiros 256 setores = 128 KiB)
  LBA 2048 .. 2055  diretorio do PROCFS: cabecalho + ate 63 entradas
  LBA 2056 ..       conteudo dos arquivos, cada um comecando num setor novo

No setor de boot este script tambem preenche:
  0x1B0  quantos blocos de 64 setores do PROCFS a BIOS deve copiar para a
         RAM (usado ao bootar por pendrive, onde o kernel nao tem driver)
  0x1B8  assinatura do disco
  0x1BE  tabela de particoes com uma particao ativa: sem ela muitas BIOS
         nao dao boot por pendrive (ou o tratam como disquete)

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

BOOT_FS_CHUNKS = 0x1B0          # deve bater com fs_chunks em boot/boot.asm
CHUNK_SECTORS = 64
RAMDISK_MAX_CHUNKS = 12         # 0x30000-0x90000 = 384 KiB copiados pela BIOS
PARTITION_TYPE = 0xDA           # "dados, sem sistema de arquivos"


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

    # quanto do PROCFS a BIOS copia para a RAM no boot (boot por pendrive)
    fs_sectors = DIR_SECTORS + len(data) // SECTOR
    chunks = -(-fs_sectors // CHUNK_SECTORS)
    if chunks > RAMDISK_MAX_CHUNKS:
        print(f"  aviso: arquivos ocupam {fs_sectors // 2} KiB; num boot por pendrive so os "
              f"primeiros {RAMDISK_MAX_CHUNKS * CHUNK_SECTORS // 2} KiB ficam disponiveis")
        chunks = RAMDISK_MAX_CHUNKS
    image[BOOT_FS_CHUNKS] = chunks

    # assinatura do disco + uma particao ativa cobrindo o resto da imagem
    total_sectors = len(image) // SECTOR
    image[0x1B8:0x1BC] = b"PROC"
    struct.pack_into("<B3sB3sII", image, 0x1BE,
                     0x80,                      # ativa (inicializavel)
                     bytes([0x00, 0x02, 0x00]), # CHS do inicio (LBA 1)
                     PARTITION_TYPE,
                     bytes([0xFE, 0xFF, 0xFF]), # CHS do fim: "use o LBA"
                     1, total_sectors - 1)

    with open(out, "wb") as f:
        f.write(image)
    print(f"  -> {out}: {len(image) // 1024} KiB, kernel {len(kernel)} bytes, {len(files)} arquivo(s)")


if __name__ == "__main__":
    main(sys.argv)
