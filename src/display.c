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

    /* Collect the lit pixels and hand them to SDL in a single call. The
     * output is identical to one SDL_RenderFillRect per pixel, but a full
     * screen used to mean 2048 SDL calls (and a much larger vertex flush)
     * per frame. */
    SDL_Rect rects[CHIP8_DISPLAY_WIDTH * CHIP8_DISPLAY_HEIGHT];
    int count = 0;

    for (int row = 0; row < CHIP8_DISPLAY_HEIGHT; row++) {
        for (int col = 0; col < CHIP8_DISPLAY_WIDTH; col++) {
            if (framebuffer[row * CHIP8_DISPLAY_WIDTH + col] != 0) {
                rects[count].x = col * display->scale;
                rects[count].y = row * display->scale;
                rects[count].w = display->scale;
                rects[count].h = display->scale;
                count++;
            }
        }
    }
    if (count > 0) {
        SDL_RenderFillRects(display->renderer, rects, count);
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
