# =====================================================================
#  PROCESSA - mini sistema operacional x86-64 com alternancia de
#  processos pela interrupcao do timer. Tudo do zero: sem libc, sem
#  bootloader pronto, sem bibliotecas.
#
#  make          gera build/processa.img (boot BIOS) e build/usb/ (boot UEFI)
#  make run      roda no QEMU (janela + log serial no terminal)
#  make test     roda sem janela por 20 s e mostra o log serial
#  make run-uefi roda no QEMU com firmware UEFI, dando boot pela pasta build/usb
#  make clean    apaga build/
#
#  Pendrive (UEFI): formate em FAT32 e copie o CONTEUDO de build/usb para ele.
#
#  Programa novo: crie user/<nome>.c e rode "make"; ele entra no disco
#  como <nome>.elf (no PROCESSA: "run <nome>.elf").
#
#  make run ARQUIVOS="pasta/prog.elf outro.bin"
#                coloca arquivos prontos no disco (rode com "run <nome>")
# =====================================================================
AS      := nasm
CC      := gcc
LD      := ld
OBJCOPY := objcopy
PYTHON  := python3
QEMU    := qemu-system-x86_64

B    := build
IMG  := $(B)/processa.img
DISK := $(B)/disk

# Nada da biblioteca padrao, nenhum cabecalho do sistema, sem SSE (o
# kernel nao salva registradores de ponto flutuante na troca de contexto).
COMMON_CFLAGS := -std=gnu11 -O2 -Wall -Wextra \
	-ffreestanding -nostdinc -fno-builtin -fno-stack-protector \
	-fno-pic -fno-pie -fcf-protection=none -mgeneral-regs-only \
	-fno-asynchronous-unwind-tables -fno-unwind-tables \
	-fno-tree-loop-distribute-patterns

KCFLAGS := $(COMMON_CFLAGS) -mno-red-zone -mcmodel=small -Ikernel -Iboot
UCFLAGS := $(COMMON_CFLAGS) -Iuser/include

# Carregador UEFI: um executavel PE32+ (formato do Windows), entao usa o
# gcc do mingw. Tambem sem nenhuma biblioteca.
EFICC      := x86_64-w64-mingw32-gcc
EFI_CFLAGS := -std=gnu11 -O2 -Wall -Wextra -ffreestanding -nostdlib -fno-builtin \
	-fno-stack-protector -fno-stack-check -mno-stack-arg-probe -mno-red-zone \
	-mgeneral-regs-only -fno-tree-loop-distribute-patterns -Iboot

LDFLAGS := -nostdlib -static --build-id=none -z max-page-size=0x1000 -z noexecstack \
	$(shell $(LD) --help 2>/dev/null | grep -q no-warn-rwx-segments && echo --no-warn-rwx-segments)

# ---------------- kernel ----------------
KSRC_C   := $(shell find kernel -name '*.c')
KSRC_ASM := $(shell find kernel -name '*.asm')
KOBJ     := $(KSRC_C:%.c=$(B)/%.o) $(KSRC_ASM:%.asm=$(B)/%.asm.o)

# ---------------- programas (vao para o disco) ----------------
ULIB     := $(B)/user/lib/crt0.asm.o $(B)/user/lib/ulib.o

# Os 4 programas de demonstracao, um em cada formato, iniciam no boot.
PROGRAMS := $(DISK)/contador.elf $(DISK)/primos.exe $(DISK)/ola.bin $(DISK)/relogio

# Qualquer outro user/*.c vira um ELF no disco automaticamente (sem iniciar
# no boot; rode com "run <nome>.elf"). Programa novo = criar o .c e compilar.
NOVOS_C     := $(filter-out user/contador.c user/primos.c,$(wildcard user/*.c))
NOVOS_PROGS := $(NOVOS_C:user/%.c=$(DISK)/%.elf)

# Arquivos extras: entram no disco como estao, sem iniciar no boot.
ARQUIVOS ?=
$(foreach f,$(ARQUIVOS),$(if $(wildcard $(f)),,$(error ARQUIVOS: '$(f)' nao existe)))

USB := $(B)/usb

all: $(IMG) usb

$(B)/boot.bin: boot/boot.asm
	@mkdir -p $(dir $@)
	$(AS) -f bin $< -o $@

$(B)/kernel/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(KCFLAGS) -MMD -MP -c $< -o $@

$(B)/kernel/%.asm.o: kernel/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf64 $< -o $@

$(B)/kernel.elf: $(KOBJ) kernel/linker.ld
	$(LD) $(LDFLAGS) -T kernel/linker.ld -o $@ $(KOBJ)

$(B)/kernel.bin: $(B)/kernel.elf
	$(OBJCOPY) -O binary $< $@

# o mesmo kernel, ligado em 0x100000 para o boot UEFI
$(B)/kernel-uefi.elf: $(KOBJ) kernel/linker.ld
	$(LD) $(LDFLAGS) --defsym=KERNEL_BASE=0x100000 -T kernel/linker.ld -o $@ $(KOBJ)

$(B)/kernel-uefi.bin: $(B)/kernel-uefi.elf
	$(OBJCOPY) -O binary $< $@

$(B)/BOOTX64.EFI: boot/uefi.c boot/bootinfo.h
	@mkdir -p $(dir $@)
	$(EFICC) $(EFI_CFLAGS) -c boot/uefi.c -o $(B)/uefi.o
	$(EFICC) -nostdlib -shared -Wl,-dll -Wl,--subsystem,10 -e efi_main -o $@ $(B)/uefi.o

$(B)/user/%.o: user/%.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -MMD -MP -c $< -o $@

$(B)/user/%.asm.o: user/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf64 $< -o $@

$(B)/user/%.elf: $(B)/user/%.o $(ULIB) user/user.ld
	$(LD) $(LDFLAGS) -T user/user.ld -o $@ $(ULIB) $<

# ELF64 (Linux): sai direto do linker (contador.elf e os programas novos)
$(DISK)/%.elf: $(B)/user/%.elf
	@mkdir -p $(dir $@)
	cp $< $@

# PE32+ (Windows): o objcopy converte o executavel para o formato PE
$(DISK)/primos.exe: $(B)/user/primos.elf
	@mkdir -p $(dir $@)
	$(OBJCOPY) -O pei-x86-64 --image-base 0x40000000 --subsystem console $< $@

# binario puro: so codigo de maquina
$(DISK)/ola.bin: user/ola.asm
	@mkdir -p $(dir $@)
	$(AS) -f bin $< -o $@

# Mach-O 64 (macOS): cabecalho escrito a mao em assembly
$(DISK)/relogio: user/relogio.asm
	@mkdir -p $(dir $@)
	$(AS) -f bin $< -o $@

# guarda a lista do que vai no disco: se ela mudar (programa novo, programa
# apagado ou ARQUIVOS diferente), a imagem e refeita
$(B)/disco.lista: FORCE
	@mkdir -p $(dir $@)
	@echo '$(NOVOS_PROGS) $(ARQUIVOS)' | cmp -s - $@ || echo '$(NOVOS_PROGS) $(ARQUIVOS)' > $@

$(IMG): $(B)/boot.bin $(B)/kernel.bin $(PROGRAMS) $(NOVOS_PROGS) $(ARQUIVOS) $(B)/disco.lista tools/mkdisk.py
	$(PYTHON) tools/mkdisk.py $@ $(B)/boot.bin $(B)/kernel.bin $(addprefix --auto ,$(PROGRAMS)) \
		$(NOVOS_PROGS) $(ARQUIVOS)

# Conteudo do pendrive para o boot UEFI (pendrive em FAT32):
#   EFI/BOOT/BOOTX64.EFI     o carregador (o firmware procura este nome)
#   PROCESSA/KERNEL.BIN      o kernel
#   PROCESSA/INICIO/         programas que iniciam no boot
#   PROCESSA/PROGRAMAS/      os outros (rode com "run <nome>")
usb: $(B)/BOOTX64.EFI $(B)/kernel-uefi.bin $(PROGRAMS) $(NOVOS_PROGS) $(ARQUIVOS)
	rm -rf $(USB)
	mkdir -p $(USB)/EFI/BOOT $(USB)/PROCESSA/INICIO $(USB)/PROCESSA/PROGRAMAS
	cp $(B)/BOOTX64.EFI $(USB)/EFI/BOOT/BOOTX64.EFI
	cp $(B)/kernel-uefi.bin $(USB)/PROCESSA/KERNEL.BIN
	cp $(PROGRAMS) $(USB)/PROCESSA/INICIO/
	$(if $(strip $(NOVOS_PROGS) $(ARQUIVOS)),cp $(NOVOS_PROGS) $(ARQUIVOS) $(USB)/PROCESSA/PROGRAMAS/)

# ---------------- execucao ----------------
QEMU_FLAGS := -m 128M -drive format=raw,file=$(IMG),if=ide -no-reboot

# UEFI: firmware OVMF + a pasta build/usb vista como um pendrive USB FAT
OVMF       := /usr/share/ovmf/OVMF.fd
QEMU_UEFI  := -m 256M -machine q35 -bios $(OVMF) -nodefaults -vga std -no-reboot \
	-drive if=none,id=stick,format=raw,readonly=on,file=fat:$(USB) \
	-device qemu-xhci -device usb-storage,drive=stick

run: $(IMG)
	$(QEMU) $(QEMU_FLAGS) -serial stdio

test: $(IMG)
	-timeout 20 $(QEMU) $(QEMU_FLAGS) -display none -serial stdio

run-uefi: usb
	$(QEMU) $(QEMU_UEFI) -serial stdio

test-uefi: usb
	-timeout 30 $(QEMU) $(QEMU_UEFI) -display none -serial stdio

clean:
	rm -rf $(B)

.PHONY: all usb run test run-uefi test-uefi clean FORCE
.SECONDARY:

-include $(shell find $(B) -name '*.d' 2>/dev/null)
