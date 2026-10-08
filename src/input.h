#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

#include "chip8.h"

/*
 * Keyboard: translates the QWERTY keyboard to the CHIP-8 hex keypad and
 * detects the quit order (ESC).
 */
typedef struct {
    bool keys[CHIP8_KEY_COUNT]; /* true = pressed */
    bool quit;                  /* true = the emulator should exit */
} Input;

/* Sets everything to zero. */
void input_init(Input *input);

/* Processes pending SDL events and updates keys/quit. */
void input_poll(Input *input);

#endif /* INPUT_H */
