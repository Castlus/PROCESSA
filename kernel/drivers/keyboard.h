#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

typedef void (*key_callback_t)(char c);

void keyboard_init(key_callback_t on_key);

#endif
