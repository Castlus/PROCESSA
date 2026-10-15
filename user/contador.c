/* contador.elf - programa C compilado como ELF64 (formato do Linux).
 *
 * Gasta CPU de proposito (espera ocupada) e NUNCA devolve a CPU por
 * conta propria. Mesmo assim os outros processos rodam: quem tira a CPU
 * dele e a interrupcao do timer.
 *
 * Rode duas copias ("run contador.elf"): as duas mostram o MESMO endereco
 * para a variavel 'contador', mas cada uma conta por conta propria, porque
 * cada processo tem o seu proprio espaco de enderecamento. */
#include "processa.h"

static unsigned long contador;

int main(void)
{
    printf("olá! sou o PID %ld; minha variável 'contador' está em %p\n",
           sys_getpid(), &contador);

    unsigned long proximo = sys_uptime();
    for (;;) {
        proximo += 1000;
        while (sys_uptime() < proximo) {
            /* espera ocupada: fica girando aqui, sem nunca chamar yield/sleep */
        }
        contador++;
        printf("contador = %lu\n", contador);
    }
}
