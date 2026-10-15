; =====================================================================
;  PROCESSA - setor de boot (MBR, 512 bytes)
; ---------------------------------------------------------------------
;  A BIOS le o primeiro setor do disco (HD, ou pendrive) para 0x7C00 e
;  pula para ca com a CPU em MODO REAL (16 bits, igual a um 8086).
;  Este codigo:
;    1. carrega o kernel do disco (LBA 1 em diante) para 0x8000;
;    2. copia a area de arquivos (PROCFS, LBA 2048 em diante) para 0x30000.
;       No QEMU o kernel le o disco pelo proprio driver ATA; num notebook,
;       bootando por pendrive USB, nao ha disco ATA: o kernel usa esta
;       copia, feita pela BIOS (que sabe ler o pendrive);
;    3. habilita a linha A20 (acesso a memoria acima de 1 MiB);
;    4. monta tabelas de paginas que mapeiam 1 GiB "identidade"
;       (endereco virtual == endereco fisico) com paginas de 2 MiB;
;    5. liga PAE + EFER.LME + paginacao: a CPU entra em MODO LONGO
;       (64 bits) direto do modo real;
;    6. salta para o kernel (64 bits) em 0x8000.
;
;  Layout do setor (para bootar tambem por pendrive):
;    0x000  jmp + espaco de BPB: algumas BIOS escrevem aqui ao bootar USB
;    0x03E  codigo e dados
;    0x1B0  fs_chunks (preenchido pelo tools/mkdisk.py)
;    0x1B8  assinatura do disco + 0x1BE tabela de particoes (mkdisk.py):
;           uma particao ativa faz a BIOS tratar o pendrive como disco
;    0x1FE  0x55 0xAA
; =====================================================================
bits 16
org 0x7C00

KERNEL_ADDR     equ 0x8000
KERNEL_SECTORS  equ 256             ; 128 KiB (tools/mkdisk.py garante que cabe)
CHUNK           equ 64              ; setores por leitura (32 KiB)
FS_LBA          equ 2048            ; inicio do PROCFS no disco
RAMDISK_ADDR    equ 0x30000         ; copia do PROCFS (ate 0x90000)
BOOTINFO        equ 0x0600          ; recado para o kernel (kernel/boot.h)
PML4            equ 0x1000          ; tabelas de paginas temporarias
PDPT            equ 0x2000
PD              equ 0x3000

    jmp short start
    nop
    times 0x3E - ($ - $$) db 0      ; espaco do BPB

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld
    sti
    mov [BOOTINFO + 12], dl         ; a BIOS informa o disco de boot em DL

    mov si, msg_boot
    call print

    ; ---- 1. kernel: LBA 1.. -> 0x8000 ----
    mov cx, KERNEL_SECTORS / CHUNK
    call read_chunks

    ; ---- 2. arquivos (PROCFS): LBA 2048.. -> 0x30000 ----
    mov word [dap_seg], RAMDISK_ADDR >> 4
    mov dword [dap_lba], FS_LBA
    movzx cx, byte [fs_chunks]
    call read_chunks
    mov dword [BOOTINFO], 'PBOT'    ; recado para o kernel: copia feita
    movzx eax, byte [fs_chunks]
    shl eax, 6                      ; blocos * 64 = setores copiados
    mov [BOOTINFO + 4], eax
    mov dword [BOOTINFO + 8], RAMDISK_ADDR

    ; ---- 3. linha A20 ("fast A20" pela porta 0x92) ----
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al

    cli
    mov al, 0xFF                    ; mascara todas as IRQs do PIC: o kernel
    out 0x21, al                    ; ainda nao tem tabela de interrupcoes
    out 0xA1, al

    ; ---- 4. tabelas de paginas: PML4 -> PDPT -> PD (512 x 2 MiB) ----
    mov di, PML4
    xor eax, eax
    mov cx, (3 * 4096) / 4
    rep stosd
    mov dword [PML4], PDPT | 3      ; presente | gravavel
    mov dword [PDPT], PD | 3
    mov di, PD
    mov eax, 0x83                   ; presente | gravavel | pagina de 2 MiB
    mov cx, 512
.map:
    mov [di], eax
    add eax, 0x200000
    add di, 8
    loop .map

    ; ---- 5. modo longo ----
    lgdt [gdt_desc]
    mov eax, PML4
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5                  ; CR4.PAE
    mov cr4, eax
    mov ecx, 0xC0000080             ; MSR EFER
    rdmsr
    or eax, 1 << 8                  ; EFER.LME (Long Mode Enable)
    wrmsr
    mov eax, cr0
    or eax, 0x80000001              ; CR0.PG | CR0.PE
    mov cr0, eax
    jmp 0x08:long_mode              ; carrega o CS de 64 bits

; Le CX blocos de CHUNK setores com INT 13h/42h (leitura por LBA) para o
; destino e o LBA que estao no DAP, avancando os dois a cada bloco.
read_chunks:
    jcxz .done
.next:
    push cx
    mov si, dap
    mov ah, 0x42
    mov dl, [BOOTINFO + 12]
    int 0x13
    jc disk_error
    add word [dap_seg], (CHUNK * 512) >> 4
    add dword [dap_lba], CHUNK
    pop cx
    loop .next
.done:
    ret

disk_error:
    mov si, msg_err
    call print
.halt:
    hlt
    jmp .halt

print:                              ; imprime string em DS:SI pela BIOS
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    xor bx, bx
    int 0x10
    jmp print
.done:
    ret

bits 64
long_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor ax, ax
    mov fs, ax
    mov gs, ax
    mov rsp, 0x7C00
    jmp KERNEL_ADDR                 ; ---- 6. kernel! ----

align 8
gdt:
    dq 0                            ; descritor nulo
    dq 0x00AF9A000000FFFF           ; 0x08: codigo 64 bits, anel 0
    dq 0x00CF92000000FFFF           ; 0x10: dados, anel 0
gdt_end:
gdt_desc:
    dw gdt_end - gdt - 1
    dd gdt

dap:                                ; Disk Address Packet do INT 13h/42h
    db 0x10, 0
    dw CHUNK                        ; quantos setores
    dw 0                            ; offset do buffer
dap_seg:
    dw KERNEL_ADDR >> 4             ; segmento do buffer
dap_lba:
    dq 1                            ; primeiro setor (LBA)

msg_boot:   db "PROCESSA", 13, 10, 0
msg_err:    db "erro de disco", 0

    times 0x1B0 - ($ - $$) db 0     ; se o codigo crescer demais, o nasm acusa aqui
fs_chunks:
    db 0                            ; blocos de 64 setores do PROCFS (mkdisk.py)
    times 0x1FE - ($ - $$) db 0     ; assinatura do disco e particoes: mkdisk.py
    dw 0xAA55
