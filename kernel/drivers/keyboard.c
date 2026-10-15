/* Driver do teclado PS/2 (IRQ1). Cada tecla gera um "scancode" na porta
 * 0x60; traduzimos para ASCII (layout US, suficiente para os comandos). */
#include "drivers/keyboard.h"
#include "arch/io.h"
#include "arch/irq.h"

#define KBD_DATA   0x60
#define KBD_STATUS 0x64

static const char keymap[] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,  'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ',
};

static const char keymap_shift[] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,  'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,  '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ',
};

static bool shift;
static key_callback_t key_callback;

static void keyboard_irq(struct regs *r)
{
    (void)r;
    uint8_t sc = inb(KBD_DATA);
    if (sc == 0xE0)                     /* prefixo de tecla estendida */
        return;

    bool released = sc & 0x80;
    sc &= 0x7F;
    if (sc == 0x2A || sc == 0x36) {     /* shift esquerdo/direito */
        shift = !released;
        return;
    }
    if (released || sc >= sizeof keymap)
        return;

    char c = shift ? keymap_shift[sc] : keymap[sc];
    if (c && key_callback)
        key_callback(c);
}

void keyboard_init(key_callback_t on_key)
{
    key_callback = on_key;
    while (inb(KBD_STATUS) & 1)         /* descarta teclas antigas */
        inb(KBD_DATA);
    irq_register(1, keyboard_irq);
}
