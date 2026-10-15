; =====================================================================
;  PROCESSA - setor de boot (MBR, 512 bytes)
; ---------------------------------------------------------------------
;  A BIOS le o primeiro setor do disco para 0x7C00 e pula para ca com a
;  CPU em MODO REAL (16 bits, igual a um 8086). Este codigo:
;    1. carrega o kernel do disco (LBA 1 em diante) para 0x8000;
;    2. habilita a linha A20 (acesso a memoria acima de 1 MiB);
;    3. monta tabelas de paginas que mapeiam 1 GiB "identidade"
;       (endereco virtual == endereco fisico) com paginas de 2 MiB;
;    4. liga PAE + EFER.LME + paginacao: a CPU entra em MODO LONGO
;       (64 bits) direto do modo real;
;    5. salta para o kernel (64 bits) em 0x8000.
; =====================================================================
bits 16
org 0x7C00

KERNEL_ADDR     equ 0x8000
KERNEL_SECTORS  equ 256             ; 128 KiB (tools/mkdisk.py garante que cabe)
CHUNK           equ 64              ; setores por leitura (32 KiB)
PML4            equ 0x1000          ; tabelas de paginas temporarias
PDPT            equ 0x2000
PD              equ 0x3000

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld
    sti
    mov [boot_drive], dl            ; a BIOS informa o disco de boot em DL

    mov si, msg_boot
    call print

    ; ---- 1. carrega o kernel com INT 13h / AH=42h (leitura por LBA) ----
    mov cx, KERNEL_SECTORS / CHUNK
.load:
    push cx
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    add word [dap_seg], (CHUNK * 512) >> 4
    add dword [dap_lba], CHUNK
    pop cx
    loop .load

    ; ---- 2. linha A20 ("fast A20" pela porta 0x92) ----
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al

    cli
    mov al, 0xFF                    ; mascara todas as IRQs do PIC: o kernel
    out 0x21, al                    ; ainda nao tem tabela de interrupcoes
    out 0xA1, al

    ; ---- 3. tabelas de paginas: PML4 -> PDPT -> PD (512 x 2 MiB) ----
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

    ; ---- 4. modo longo ----
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
    jmp KERNEL_ADDR                 ; ---- 5. kernel! ----

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

boot_drive: db 0
msg_boot:   db "PROCESSA: carregando o kernel...", 13, 10, 0
msg_err:    db "PROCESSA: erro lendo o disco!", 0

times 510 - ($ - $$) db 0
dw 0xAA55
