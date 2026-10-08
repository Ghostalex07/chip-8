#include "display.h"

#include <stdio.h>

bool display_init(Display *display, int scale)
{
    display->scale = scale;
    display->window = NULL;
    display->renderer = NULL;

    display->window = SDL_CreateWindow(
        "CHIP-8",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        CHIP8_DISPLAY_WIDTH * scale,
        CHIP8_DISPLAY_HEIGHT * scale,
        SDL_WINDOW_SHOWN);
    if (display->window == NULL) {
        fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
        return false;
    }

    /* Accelerated if there is a GPU; otherwise software (useful headless). */
    display->renderer = SDL_CreateRenderer(display->window, -1, SDL_RENDERER_ACCELERATED);
    if (display->renderer == NULL) {
        display->renderer = SDL_CreateRenderer(display->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (display->renderer == NULL) {
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(display->window);
        display->window = NULL;
        return false;
    }

    return true;
}

void display_render(Display *display, const uint8_t *framebuffer)
{
    SDL_SetRenderDrawColor(display->renderer, 0, 0, 0, 255);
    SDL_RenderClear(display->renderer);

    SDL_SetRenderDrawColor(display->renderer, 255, 255, 255, 255);
    SDL_Rect rect;
    rect.w = display->scale;
    rect.h = display->scale;

    for (int row = 0; row < CHIP8_DISPLAY_HEIGHT; row++) {
        for (int col = 0; col < CHIP8_DISPLAY_WIDTH; col++) {
            if (framebuffer[row * CHIP8_DISPLAY_WIDTH + col] != 0) {
                rect.x = col * display->scale;
                rect.y = row * display->scale;
                SDL_RenderFillRect(display->renderer, &rect);
            }
        }
    }

    SDL_RenderPresent(display->renderer);
}

void display_destroy(Display *display)
{
    if (display->renderer != NULL) {
        SDL_DestroyRenderer(display->renderer);
        display->renderer = NULL;
    }
    if (display->window != NULL) {
        SDL_DestroyWindow(display->window);
        display->window = NULL;
    }
}
