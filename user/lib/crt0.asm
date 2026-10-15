; crt0: primeiro codigo de um programa em C. O kernel pula para _start;
; chamamos main() e, quando ela retornar, pedimos ao kernel para sair.
bits 64
section .text
global _start
extern main

_start:
    xor ebp, ebp
    call main
    mov rdi, rax                    ; codigo de saida = retorno de main
    mov rax, 0                      ; SYS_EXIT
    int 0x80
.hang:
    jmp .hang
