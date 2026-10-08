#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <SDL.h>

#include "chip8.h"

/*
 * Window and rendering of the 64×32 screen with SDL2.
 * The framebuffer is the display[] array of Chip8: one byte per pixel,
 * 0 = off and 1 = on.
 */
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    int scale; /* window pixels per CHIP-8 pixel */
} Display;

/* Creates the window (64·scale × 32·scale). Returns false on failure. */
bool display_init(Display *display, int scale);

/* Draws the whole framebuffer into the window. */
void display_render(Display *display, const uint8_t *framebuffer);

/* Releases window and renderer. */
void display_destroy(Display *display);

#endif /* DISPLAY_H */
