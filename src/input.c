#include "input.h"

#include <SDL.h>

/*
 * CHIP-8 hex keypad (values 0x0–0xF) indexed by the QWERTY key that
 * activates it:
 *
 *   CHIP-8:   1 2 3 C     Keyboard:  1 2 3 4
 *             4 5 6 D                Q W E R
 *             7 8 9 E                A S D F
 *             A 0 B F                Z X C V
 */
static const SDL_Scancode keymap[CHIP8_KEY_COUNT] = {
    [0x0] = SDL_SCANCODE_X,
    [0x1] = SDL_SCANCODE_1,
    [0x2] = SDL_SCANCODE_2,
    [0x3] = SDL_SCANCODE_3,
    [0x4] = SDL_SCANCODE_Q,
    [0x5] = SDL_SCANCODE_W,
    [0x6] = SDL_SCANCODE_E,
    [0x7] = SDL_SCANCODE_A,
    [0x8] = SDL_SCANCODE_S,
    [0x9] = SDL_SCANCODE_D,
    [0xA] = SDL_SCANCODE_Z,
    [0xB] = SDL_SCANCODE_C,
    [0xC] = SDL_SCANCODE_4,
    [0xD] = SDL_SCANCODE_R,
    [0xE] = SDL_SCANCODE_F,
    [0xF] = SDL_SCANCODE_V,
};

void input_init(Input *input)
{
    for (int k = 0; k < CHIP8_KEY_COUNT; k++) {
        input->keys[k] = false;
    }
    input->quit = false;
}

/* Returns the CHIP-8 key index, or -1. */
static int scancode_to_chip8(SDL_Scancode scancode)
{
    for (int k = 0; k < CHIP8_KEY_COUNT; k++) {
        if (keymap[k] == scancode) {
            return k;
        }
    }
    return -1;
}

void input_poll(Input *input)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            input->quit = true;
            break;

        case SDL_KEYDOWN:
            if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                input->quit = true;
                break;
            }
            {
                int key = scancode_to_chip8(event.key.keysym.scancode);
                if (key >= 0) {
                    input->keys[key] = true;
                }
            }
            break;

        case SDL_KEYUP:
            {
                int key = scancode_to_chip8(event.key.keysym.scancode);
                if (key >= 0) {
                    input->keys[key] = false;
                }
            }
            break;

        default:
            break;
        }
    }
}
