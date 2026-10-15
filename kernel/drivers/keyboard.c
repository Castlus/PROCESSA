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

static void handle_scancode(uint8_t sc)
{
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

static void keyboard_irq(struct regs *r)
{
    (void)r;
    handle_scancode(inb(KBD_DATA));
}

/* Modo polling (boot UEFI): o timer chama isto a cada tick. */
void keyboard_poll(void)
{
    for (int i = 0; i < 16; i++) {
        uint8_t st = inb(KBD_STATUS);
        if (st == 0xFF || !(st & 1))    /* sem controlador / nada novo */
            return;
        uint8_t sc = inb(KBD_DATA);
        if (st & 0x20)                  /* byte do mouse/touchpad: ignora */
            continue;
        handle_scancode(sc);
    }
}

void keyboard_init(key_callback_t on_key, bool use_irq)
{
    key_callback = on_key;
    for (int i = 0; i < 64; i++) {      /* descarta teclas antigas */
        uint8_t st = inb(KBD_STATUS);
        if (st == 0xFF || !(st & 1))
            break;
        inb(KBD_DATA);
    }
    if (use_irq)
        irq_register(1, keyboard_irq);
}
