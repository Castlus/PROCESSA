/* Driver da porta serial COM1 (UART 16550). Todo o log do kernel tambem
 * sai por aqui; o QEMU mostra no terminal com "-serial stdio". */
#include "drivers/serial.h"
#include "arch/io.h"

#define COM1 0x3F8

void serial_init(void)
{
    outb(COM1 + 1, 0x00);   /* sem interrupcoes                   */
    outb(COM1 + 3, 0x80);   /* DLAB=1: vai configurar a velocidade */
    outb(COM1 + 0, 0x03);   /* divisor 3 -> 38400 baud             */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);   /* 8 bits, sem paridade, 1 stop bit    */
    outb(COM1 + 2, 0xC7);   /* FIFO ligada                         */
    outb(COM1 + 4, 0x03);   /* DTR + RTS                           */
}

void serial_putc(char c)
{
    if (c == '\n')
        serial_putc('\r');
    for (int i = 0; i < 100000 && !(inb(COM1 + 5) & 0x20); i++)
        ;
    outb(COM1, (uint8_t)c);
}
