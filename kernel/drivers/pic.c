/* Driver do PIC 8259 (controlador de interrupcoes).
 *
 * Os dispositivos nao falam direto com a CPU: cada um tem uma linha de
 * IRQ ligada ao PIC, e o PIC repassa para a CPU como um numero de
 * interrupcao. Por padrao as IRQs 0-7 caem nos vetores 8-15, que colidem
 * com as excecoes da CPU, entao remapeamos para 32-47. */
#include "drivers/pic.h"
#include "arch/io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20

void pic_init(void)
{
    outb(PIC1_CMD, 0x11);  io_wait();   /* ICW1: inicializar, havera ICW4     */
    outb(PIC2_CMD, 0x11);  io_wait();
    outb(PIC1_DATA, 0x20); io_wait();   /* ICW2: IRQ 0-7  -> vetores 32-39    */
    outb(PIC2_DATA, 0x28); io_wait();   /* ICW2: IRQ 8-15 -> vetores 40-47    */
    outb(PIC1_DATA, 0x04); io_wait();   /* ICW3: escravo ligado na IRQ2       */
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();   /* ICW4: modo 8086                    */
    outb(PIC2_DATA, 0x01); io_wait();

    outb(PIC1_DATA, 0xFF);              /* tudo mascarado: cada driver libera */
    outb(PIC2_DATA, 0xFF);              /* a sua IRQ quando se registra       */
}

void pic_unmask(int irq)
{
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) {
        pic_unmask(2);                  /* cascata mestre -> escravo */
        irq -= 8;
    }
    outb(port, inb(port) & ~(1 << irq));
}

void pic_eoi(int irq)
{
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

/* IRQ 7/15 podem ser "fantasmas" (ruido eletrico). Confere no registrador
 * ISR do PIC se a interrupcao era de verdade. */
bool pic_is_spurious(int irq)
{
    if (irq != 7 && irq != 15)
        return false;
    uint16_t cmd = irq == 7 ? PIC1_CMD : PIC2_CMD;
    outb(cmd, 0x0B);
    if (inb(cmd) & 0x80)
        return false;
    if (irq == 15)
        outb(PIC1_CMD, PIC_EOI);
    return true;
}
