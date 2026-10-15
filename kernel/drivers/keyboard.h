#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

#include "types.h"

typedef void (*key_callback_t)(char c);

/* use_irq: o teclado avisa pela IRQ1 (boot BIOS). Sem IRQ (boot UEFI), o
 * kernel chama keyboard_poll() a cada tick do timer. */
void keyboard_init(key_callback_t on_key, bool use_irq);
void keyboard_poll(void);

#endif
