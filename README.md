# PROCESSA

Mini sistema operacional x86-64 feito do zero que carrega programas do disco em
**qualquer um de quatro formatos de executável** e alterna entre eles usando a
**interrupção do timer de hardware** (multitarefa preemptiva).

Não usa bibliotecas: sem libc, sem bootloader pronto (GRUB/Limine), sem
cabeçalhos do sistema (`-nostdinc -nostdlib`). Tudo, do setor de boot ao
`printf`, está neste repositório. Usamos só as ferramentas de compilação
(gcc, nasm, ld, objcopy) e um script Python que monta a imagem de disco.

![tela do PROCESSA](docs/tela.png)

## O que o desafio pede e onde está

| Requisito | Implementação |
|---|---|
| Definir o que é um processo | `struct process` (o PCB) em [kernel/proc/process.h](kernel/proc/process.h): pid, estado, registradores salvos, tabela de páginas própria (CR3), pilha de kernel, tempo de CPU |
| Ler vários processos / carregar do disco | driver de disco ATA ([kernel/drivers/ata.c](kernel/drivers/ata.c)) + sistema de arquivos PROCFS ([kernel/fs/procfs.c](kernel/fs/procfs.c)) + loader ([kernel/loader/loader.c](kernel/loader/loader.c)) |
| Qualquer formato binário | o loader descobre o formato pelos **primeiros bytes do arquivo** (número mágico), não pela extensão: **ELF64** (Linux), **PE32+** (Windows), **Mach-O 64** (macOS) e **binário puro**. Cada formato é um módulo em [kernel/loader/](kernel/loader/) |
| Arquitetura de 64 bits | o setor de boot leva a CPU do modo real (16 bits) direto ao modo longo (64 bits): [boot/boot.asm](boot/boot.asm) |
| Alternar usando o driver do timer | driver do PIT 8254 ([kernel/drivers/pit.c](kernel/drivers/pit.c)) gera a IRQ0 100x por segundo; a cada tick o escalonador round-robin ([kernel/proc/sched.c](kernel/proc/sched.c)) decide se troca de processo, e a troca acontece em [kernel/arch/isr.asm](kernel/arch/isr.asm) |

Também tem:

- **Isolamento de memória:** cada processo tem a sua própria tabela de páginas e roda em anel 3 (modo usuário).
- **Chamadas de sistema** via `int 0x80`.
- **Painel ao vivo** com os PCBs, o tick do timer e a fatia de tempo restante do processo na CPU.
- **Shell** com os comandos `ls`, `run`, `kill`, `ps`, `info`, `trace`, `quantum`, `freq` e `mem`.

## Como compilar e rodar

### Opção A: WSL (Ubuntu)

```bash
sudo apt install build-essential nasm python3 qemu-system-x86 gcc-mingw-w64-x86-64 ovmf
make run        # compila e abre o QEMU
```

### Opção B: Windows com Docker Desktop + QEMU para Windows

```powershell
.\build.ps1                                          # compila dentro de um container Ubuntu
winget install SoftwareFreedomConservancy.QEMU       # uma vez só
.\run.ps1                                            # abre o QEMU com a imagem
```

Outros alvos: `make test` roda 20 s sem janela e mostra o log serial; `make clean` apaga `build/`.

### Opção C: num PC de verdade, por pendrive (UEFI)

O PROCESSA dá boot direto num computador, sem sistema operacional nenhum por baixo. Este é o caminho para PCs atuais, que só têm UEFI.

1. **Formate o pendrive em FAT32.**
   - Até 32 GB: pelo próprio Windows (botão direito → Formatar → FAT32).
   - Maiores: use o [Rufus](https://rufus.ie) com "Seleção de boot: Não inicializável" e "Sistema de arquivos: FAT32".
2. **Copie o PROCESSA:** `.\pendrive.ps1 E:` (troque `E:` pela letra do pendrive).
   - O script compila e copia o conteúdo de `build\usb\` para o pendrive.
   - Ele só copia arquivos e se recusa a escrever em algo que não seja um pendrive em FAT32.
3. **Na configuração do firmware do PC, desligue o Secure Boot.** O nosso carregador não é assinado pela Microsoft.
4. **Ligue pelo menu de boot** (F12, F9, F8 ou Esc, conforme a marca) e escolha o pendrive.

**O que vai no pendrive:**

| Caminho | Conteúdo |
|---|---|
| `EFI\BOOT\BOOTX64.EFI` | o carregador UEFI ([boot/uefi.c](boot/uefi.c)), que o firmware procura por esse nome |
| `PROCESSA\KERNEL.BIN` | o kernel |
| `PROCESSA\INICIO\` | programas que iniciam no boot |
| `PROCESSA\PROGRAMAS\` | outros programas: é só **arrastar arquivos para cá** no Windows e rodar com `run <nome>` |

**Como funciona o boot UEFI:**
- **Carregamento:** o firmware UEFI sabe ler FAT32 e executa o `BOOTX64.EFI`. É um executável PE32+ escrito do zero, sem gnu-efi/EDK2. Ele lê o kernel e os programas pelo protocolo de arquivos do firmware e monta com eles um disco PROCFS na memória. Depois pega a tela gráfica, o mapa de memória e a frequência do TSC, chama `ExitBootServices` e salta para o kernel.
- **Vídeo:** em UEFI não existe o modo texto da VGA. O kernel desenha a mesma grade de 80x25 pixel a pixel na tela gráfica, com a fonte 8x16 da VGA ([kernel/drivers/font8x16.c](kernel/drivers/font8x16.c), extraída da VGA BIOS do QEMU por [tools/extrair_fonte.py](tools/extrair_fonte.py)).
- **Timer:** sem o modo Legacy, muitos PCs desligam o chip PIT. O kernel usa o **timer do APIC local** da CPU ([kernel/drivers/lapic.c](kernel/drivers/lapic.c)), calibrado contra o TSC. Ele continua sendo um driver de timer gerando a interrupção que troca os processos.
- **Teclado:** é lido a cada tick do timer (polling), porque a IRQ1 do PIC antigo pode não chegar à CPU.
- **Aviso de timer:** se nenhum timer gerar interrupções, o kernel avisa em vermelho no boot.

**Testar no QEMU antes:** `make run-uefi` (WSL) dá boot com firmware UEFI (OVMF), usando a pasta `build/usb` como se fosse o pendrive.

**PCs antigos, só com BIOS/Legacy:** grave `build\processa.img` no pendrive com o Rufus (modo DD), o que apaga o pendrive, e dê boot pela entrada sem "UEFI:". Nesse caminho, o setor de boot pede à BIOS para copiar os arquivos para a memória.

### Escrevendo um programa novo

Crie um arquivo `.c` em `user/` e compile normalmente. Ele entra no disco sozinho como `<nome>.elf`:

```c
/* user/oi.c */
#include "processa.h"

int main(void)
{
    printf("oi! sou o PID %ld\n", sys_getpid());
    for (int i = 1; i <= 5; i++) {
        printf("volta %d de 5\n", i);
        sys_sleep(1000);
    }
    return 0;
}
```

```powershell
.\build.ps1        # ou: make
.\run.ps1          # no PROCESSA: run oi.elf
```

O programa só tem o que está em [user/include/processa.h](user/include/processa.h):

- `printf`, `print`, `strlen`, `memset` e `memcpy`;
- as chamadas de sistema `sys_getpid`, `sys_sleep`, `sys_uptime`, `sys_yield` e `sys_exit`.

Ele não lê o teclado, não usa ponto flutuante e não tem `malloc`.

### Colocando arquivos prontos no disco

Para colocar qualquer arquivo no disco virtual, use a variável `ARQUIVOS`:

```bash
make run ARQUIVOS="extras/prog.elf extras/outro.bin"        # WSL
.\build.ps1 -Arquivos "extras\prog.elf extras\outro.bin"     # Windows (depois .\run.ps1)
```

- Os arquivos entram no disco com o mesmo nome (sem a pasta) e não iniciam no boot: rode com `run <nome>`.
- No Windows, eles precisam estar dentro da pasta do projeto, porque é ela que o container do Docker enxerga.
- Nomes com espaço não são aceitos.

## Comandos (digite na linha `processa>`)

| Comando | O que faz |
|---|---|
| `ls` | lista os arquivos do disco e o formato detectado de cada um |
| `run <arquivo>` | carrega o arquivo do disco e cria um processo novo (pode repetir) |
| `kill <pid>` | encerra um processo |
| `ps` | imprime a tabela de processos e a fila de prontos (ordem do round-robin) |
| `info <pid>` | mostra o PCB inteiro: registradores salvos, pilha de kernel, CR3 e faixas de memória mapeadas |
| `trace on\|off` | registra cada troca de contexto: tick, quem saiu, quem entrou, o motivo, onde parou e onde retoma (rip) |
| `saida on\|off` | esconde/mostra a saída dos processos na tela (eles continuam rodando; o log serial recebe tudo) |
| `quantum <n>` | muda a fatia de tempo (em ticks do timer) |
| `freq <hz>` | reprograma o chip do timer (19 a 1000 Hz) |
| `mem` | mostra a memória física em uso |
| `limpar` | limpa o log |

## Programas de exemplo (no disco)

| Arquivo | Formato | Comportamento |
|---|---|---|
| `contador.elf` | ELF64, gerado pelo gcc/ld | gasta CPU sem parar e nunca devolve a CPU; só a interrupção do timer o tira da CPU |
| `primos.exe` | PE32+, convertido pelo objcopy | calcula primos por 30 s (só CPU) e termina sozinho |
| `ola.bin` | binário puro, sem cabeçalho | imprime e dorme 1,5 s (estado DORMINDO) |
| `relogio` | Mach-O 64, cabeçalho escrito à mão em assembly | imprime o tempo desde o boot a cada 2 s; não tem extensão, o kernel reconhece pelo conteúdo |

## Como funciona

```
BIOS -> boot.asm (16 bits) -> modo longo (64 bits) -> kmain()
  kmain: GDT/TSS, IDT, PIC, memória, timer, teclado, disco, PROCFS
         carrega os programas do disco -> PCBs no estado PRONTO
         liga as interrupções e vira o processo ocioso (PID 0)

a cada IRQ0 do timer (10 ms):
  isr.asm salva os registradores do processo atual na pilha de kernel dele
  isr_handler -> sched_tick(): acabou a fatia? -> schedule():
      atual vira PRONTO, o próximo vira EXECUTANDO
      TSS.rsp0 = pilha de kernel do próximo; CR3 = tabela de páginas do próximo
  isr.asm troca o RSP para o contexto do próximo, restaura os registradores, iretq
  -> o próximo processo continua exatamente de onde parou
```

### Mapa de memória de cada processo

| Faixa | Conteúdo |
|---|---|
| `0x00000000 - 0x3FFFFFFF` | kernel (compartilhado, só acessível em anel 0) |
| `0x40000000 - 0x7FFEFFFF` | código e dados do programa |
| `0x7FFF0000 - 0x7FFFFFFF` | pilha do processo |

### Layout do disco

| Setor (LBA) | Conteúdo |
|---|---|
| 0 | setor de boot, com uma partição ativa que cobre o resto do disco (para a BIOS aceitar dar boot pelo pendrive) |
| 1 | kernel |
| 2048 | diretório PROCFS |
| 2056+ | arquivos |

## Estrutura

```
boot/boot.asm          setor de boot (BIOS): modo real -> modo longo, carrega o kernel
boot/uefi.c            carregador UEFI (BOOTX64.EFI): lê kernel e programas do pendrive
boot/bootinfo.h        o que o carregador UEFI entrega ao kernel
kernel/
  arch/                entrada 64 bits, GDT/TSS, IDT, stubs de interrupção (troca de contexto)
  drivers/             timer (PIT e APIC), PIC, ATA (disco), teclado, vídeo (texto e gráfico), serial
  mm/                  memória física (bitmap) e virtual (paginação de 4 níveis)
  fs/                  PROCFS (diretório simples no disco)
  loader/              ELF64, PE32+, Mach-O 64, binário puro
  proc/                PCB, escalonador round-robin, chamadas de sistema
  lib/                 memset/memcpy/printf próprios
  ui.c shell.c         painel e linha de comando
  kernel.c             kmain
user/                  programas de exemplo + crt0 + mini biblioteca própria
tools/mkdisk.py        monta a imagem de disco (boot BIOS)
pendrive.ps1           copia o PROCESSA para um pendrive FAT32 (boot UEFI)
```

## Limitações conhecidas

- **Uma CPU só**, e o kernel roda com as interrupções desligadas, então não precisa de travas (locks).
- **Sem ponto flutuante nos processos.** O kernel não salva os registradores FPU/SSE na troca, por isso tudo é compilado com `-mgeneral-regs-only`.
- **Até 16 processos.** A memória de processos tem 62 MiB, entre 2 e 64 MiB; o QEMU roda com 128 MiB.
- **ELF:** só executável estático (`ET_EXEC`).
- **PE:** sem importação de DLLs e carregado no `ImageBase`, sem realocação.
- **Mach-O:** executável x86-64 (`MH_EXECUTE`).
- **Teclado:** layout US.
