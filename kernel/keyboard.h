#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_initialize(void);
void keyboard_handle_scancode(uint8_t scancode);

#endif