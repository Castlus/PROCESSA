; Ponto de entrada do kernel (ja em 64 bits). Quem salta para ca:
;   - o setor de boot (boot.asm), no boot por BIOS;
;   - o carregador UEFI (uefi.c), que passa as informacoes de boot em RDI.
bits 64

section .text.entry progbits alloc exec nowrite align=16
global _start
extern kmain
extern __bss_start
extern __bss_end

_start:
    cli
    mov [rel boot_arg], rdi         ; guarda antes de zerar a .bss
    mov rdi, __bss_start            ; zera a .bss (variaveis globais sem valor inicial)
    mov rcx, __bss_end
    sub rcx, rdi
    xor eax, eax
    rep stosb
    mov rsp, boot_stack_top         ; pilha do kernel (e do processo "ocioso", PID 0)
    xor ebp, ebp
    mov rdi, [rel boot_arg]         ; kmain(boot_arg)
    call kmain
.halt:
    cli
    hlt
    jmp .halt

section .data
boot_arg:
    dq 0

section .bss
align 16
boot_stack:
    resb 16384
boot_stack_top:
