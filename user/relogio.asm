; relogio - executavel Mach-O 64 (formato do macOS) montado byte a byte.
; Nao existe linker de Mach-O no Linux, entao escrevemos o cabecalho e os
; "load commands" na mao. O arquivo nao tem extensao: o kernel reconhece
; o formato pelo numero magico 0xFEEDFACF, nao pelo nome.
bits 64
org 0x40000000

MH_MAGIC_64        equ 0xFEEDFACF
CPU_TYPE_X86_64    equ 0x01000007
CPU_SUBTYPE_ALL    equ 3
MH_EXECUTE         equ 2
LC_SEGMENT_64      equ 0x19
LC_UNIXTHREAD      equ 0x5
X86_THREAD_STATE64 equ 4

SYS_WRITE  equ 1
SYS_SLEEP  equ 4
SYS_UPTIME equ 5

; ---------------- mach_header_64 ----------------
header:
    dd MH_MAGIC_64
    dd CPU_TYPE_X86_64
    dd CPU_SUBTYPE_ALL
    dd MH_EXECUTE                   ; filetype
    dd 2                            ; ncmds
    dd cmds_end - cmds              ; sizeofcmds
    dd 1                            ; flags (MH_NOUNDEFS)
    dd 0                            ; reserved

cmds:
; ---------------- LC_SEGMENT_64 "__TEXT" ----------------
segment_cmd:
    dd LC_SEGMENT_64
    dd segment_end - segment_cmd
    db "__TEXT", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    dq 0x40000000                   ; vmaddr
    dq file_end - header            ; vmsize
    dq 0                            ; fileoff
    dq file_end - header            ; filesize
    dd 7                            ; maxprot  (rwx)
    dd 7                            ; initprot (rwx)
    dd 1                            ; nsects
    dd 0                            ; flags
    ; section_64 "__text"
    db "__text", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db "__TEXT", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    dq code                         ; addr
    dq file_end - code              ; size
    dd code - header                ; offset
    dd 4                            ; align (2^4)
    dd 0, 0                         ; reloff, nreloc
    dd 0x80000400                   ; flags: so instrucoes
    dd 0, 0, 0                      ; reserved1-3
segment_end:

; ---------------- LC_UNIXTHREAD (registradores iniciais) ----------------
thread:
    dd LC_UNIXTHREAD
    dd thread_end - thread
    dd X86_THREAD_STATE64
    dd 42                           ; 21 registradores de 64 bits
    times 16 dq 0                   ; rax, rbx, rcx, rdx, rdi, rsi, rbp, rsp, r8-r15
    dq code                         ; rip = PONTO DE ENTRADA
    times 4 dq 0                    ; rflags, cs, fs, gs
thread_end:
cmds_end:

; ---------------- codigo ----------------
align 16
code:
.loop:
    mov rax, SYS_UPTIME             ; rax = milissegundos desde o boot
    int 0x80
    xor edx, edx
    mov rcx, 1000
    div rcx                         ; rax = segundos

    lea rdi, [rel segundos + 4]     ; escreve 5 digitos, do fim para o inicio
    mov rbx, 10
    mov rcx, 5
.digito:
    xor edx, edx
    div rbx
    add dl, '0'
    mov [rdi], dl
    dec rdi
    loop .digito

    mov rax, SYS_WRITE
    lea rdi, [rel msg]
    mov rsi, msg_len
    int 0x80

    mov rax, SYS_SLEEP              ; dorme 2 s
    mov rdi, 2000
    int 0x80
    jmp .loop

msg:      db "tic-tac do Mach-O: "
segundos: db "00000"
          db " s desde o boot", 10
msg_len   equ $ - msg

file_end:
