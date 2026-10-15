/* primos.exe - programa C empacotado como PE32+ (formato .exe do Windows).
 *
 * Processo "CPU-bound": so faz conta, sem pausas, durante DURACAO_MS.
 * Depois termina sozinho (mostra o estado TERMINADO e a memoria sendo
 * devolvida ao kernel). */
#include "processa.h"

#define DURACAO_MS  30000
#define RELATORIO_MS 3000

static int eh_primo(unsigned long n)
{
    if (n < 2)
        return 0;
    if (n % 2 == 0)
        return n == 2;
    for (unsigned long d = 3; d * d <= n; d += 2)
        if (n % d == 0)
            return 0;
    return 1;
}

int main(void)
{
    unsigned long inicio = sys_uptime();
    unsigned long relatorio = inicio + RELATORIO_MS;
    unsigned long achados = 0, maior = 0;

    printf("vou procurar números primos por %d s (só CPU, sem pausas)\n", DURACAO_MS / 1000);
    for (unsigned long n = 2;; n++) {
        if (eh_primo(n)) {
            achados++;
            maior = n;
        }
        if (n % 1024)
            continue;

        unsigned long agora = sys_uptime();
        if (agora >= relatorio) {
            printf("%lu primos até agora; o maior é %lu\n", achados, maior);
            relatorio += RELATORIO_MS;
        }
        if (agora - inicio >= DURACAO_MS)
            break;
    }
    printf("terminei: %lu primos em %d s. tchau!\n", achados, DURACAO_MS / 1000);
    return 0;
}
