# Ambiente de build do PROCESSA (alternativa ao WSL).
#   docker build -t processa-build .
#   docker run --rm -v "$PWD:/src" processa-build make
FROM ubuntu:24.04
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      make gcc binutils nasm python3 qemu-system-x86 \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
