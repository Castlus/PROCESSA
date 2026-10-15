; ola.bin - BINARIO PURO: sem cabecalho nenhum, so codigo de maquina x86-64.
; O kernel copia o arquivo para 0x40000000 e pula para o primeiro byte.
;
; Este processo DORME entre uma mensagem e outra (sys_sleep): enquanto
; dorme ele fica no estado DORMINDO e nao disputa a CPU.
bits 64
org 0x40000000

SYS_WRITE equ 1
SYS_SLEEP equ 4

start:
    xor r12, r12                    ; contador de voltas (o kernel preserva r12)
.loop:
    mov rax, r12                    ; digito = voltas % 10
    xor edx, edx
    mov rcx, 10
    div rcx
    add dl, '0'
    mov [digito], dl

    mov rax, SYS_WRITE
    lea rdi, [rel msg]
    mov rsi, msg_len
    int 0x80

    mov rax, SYS_SLEEP              ; dorme 1,5 s
    mov rdi, 1500
    int 0x80

    inc r12
    jmp .loop

msg:    db "olá do binário puro (sem cabeçalho)! volta "
digito: db "0", 10
msg_len equ $ - msg
