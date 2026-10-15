; =====================================================================
;  Entradas de interrupcao.
;
;  TODA interrupcao - excecao da CPU, IRQ de hardware (timer, teclado)
;  ou chamada de sistema (int 0x80) - passa por isr_common, que:
;    1. salva todos os registradores na pilha de kernel do processo;
;    2. chama isr_handler (C) passando o ponteiro desse "contexto";
;    3. recebe de volta o ponteiro de contexto que deve ser retomado;
;    4. troca RSP para ele, restaura os registradores e faz iretq.
;
;  Se o C devolver o contexto de OUTRO processo, o iretq "volta" para
;  esse outro processo: e assim que a troca de contexto acontece.
; =====================================================================
bits 64
section .text
extern isr_handler

%macro ISR_NOERR 1
isr_stub_%1:
    push qword 0                    ; codigo de erro falso (quadro sempre igual)
    push qword %1                   ; numero do vetor
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr_stub_%1:                        ; a CPU ja empilhou um codigo de erro
    push qword %1
    jmp isr_common
%endmacro

; ---- 0-31: excecoes da CPU ----
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

; ---- 32-47: IRQs do PIC (32 = timer PIT, 33 = teclado, ...) ----
ISR_NOERR 32
ISR_NOERR 33
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47

; ---- 128: chamada de sistema (int 0x80) ----
global isr_stub_128
ISR_NOERR 128

isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp                    ; argumento: struct regs * (contexto salvo)
    cld
    call isr_handler                ; devolve o contexto a retomar
    mov rsp, rax                    ; <<< TROCA DE CONTEXTO: muda de pilha >>>

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16                     ; descarta vetor + codigo de erro
    iretq                           ; restaura rip, cs, rflags, rsp, ss

; Tabela com o endereco de cada entrada, usada pelo idt.c
section .data
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 48
    dq isr_stub_%+i
%assign i i+1
%endrep
