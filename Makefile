# =====================================================================
#  PROCESSA - mini sistema operacional x86-64 com alternancia de
#  processos pela interrupcao do timer. Tudo do zero: sem libc, sem
#  bootloader pronto, sem bibliotecas.
#
#  make          gera build/processa.img
#  make run      roda no QEMU (janela + log serial no terminal)
#  make test     roda sem janela por 20 s e mostra o log serial
#  make clean    apaga build/
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

KCFLAGS := $(COMMON_CFLAGS) -mno-red-zone -mcmodel=small -Ikernel
UCFLAGS := $(COMMON_CFLAGS) -Iuser/include

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

all: $(IMG)

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

# ---------------- execucao ----------------
QEMU_FLAGS := -m 128M -drive format=raw,file=$(IMG),if=ide -no-reboot

run: $(IMG)
	$(QEMU) $(QEMU_FLAGS) -serial stdio

test: $(IMG)
	-timeout 20 $(QEMU) $(QEMU_FLAGS) -display none -serial stdio

clean:
	rm -rf $(B)

.PHONY: all run test clean FORCE
.SECONDARY:

-include $(shell find $(B) -name '*.d' 2>/dev/null)
