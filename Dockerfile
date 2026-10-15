# Ambiente de build do PROCESSA (alternativa ao WSL).
#   docker build -t processa-build .
#   docker run --rm -v "$PWD:/src" processa-build make
FROM ubuntu:24.04
#   gcc-mingw-w64-x86-64: gera o carregador UEFI (BOOTX64.EFI e um executavel PE)
#   ovmf: firmware UEFI para testar o boot UEFI no QEMU
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      make gcc binutils nasm python3 qemu-system-x86 gcc-mingw-w64-x86-64 ovmf \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
