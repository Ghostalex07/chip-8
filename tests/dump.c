#include <stdio.h>
#include <stdlib.h>

#include "chip8.h"

/* Arnés de verificación: ejecuta N ciclos y vuelca el framebuffer como texto. */
int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom> [cycles]\n", argv[0]);
        return 1;
    }
    int cycles = 1000;
    if (argc > 2) {
        char *end;
        long value = strtol(argv[2], &end, 10);
        if (end == argv[2] || *end != '\0' || value <= 0 || value > 1000000000L) {
            fprintf(stderr, "Invalid cycle count: '%s'\n", argv[2]);
            return 1;
        }
        cycles = (int)value;
    }

    Chip8 c;
    chip8_init(&c);
    if (!chip8_load_rom(&c, argv[1])) {
        return 1;
    }

    /* Optional: byte to poke at 0x1FF before running (quirks test menu). */
    if (argc > 4) {
        c.memory[0x1FF] = (uint8_t)strtol(argv[4], NULL, 16);
    }

    /* Optional: hex bitmask of keys held down for the whole run. */
    unsigned long key_mask = 0;
    if (argc > 5) {
        key_mask = strtoul(argv[5], NULL, 16);
    }

    for (int i = 0; i < cycles; i++) {
        for (int k = 0; k < CHIP8_KEY_COUNT; k++) {
            c.keys[k] = (key_mask >> k) & 1;
        }
        chip8_cycle(&c);
        if (i % 10 == 9) {
            chip8_frame_end(&c);
            chip8_tick_timers(&c); /* ~60 Hz at 10 cycles per frame */
        }
    }

    int on = 0;
    for (int row = 0; row < CHIP8_DISPLAY_HEIGHT; row++) {
        for (int col = 0; col < CHIP8_DISPLAY_WIDTH; col++) {
            int px = c.display[row * CHIP8_DISPLAY_WIDTH + col];
            if (px) {
                on++;
            }
            putchar(px ? '#' : '.');
        }
        putchar('\n');
    }
    printf("pixeles encendidos: %d | PC=0x%03X | I=0x%03X\n", on, c.pc, c.i);

    if (argc > 3) {
        FILE *raw = fopen(argv[3], "wb");
        if (raw == NULL) {
            fprintf(stderr, "Could not open output file '%s'\n", argv[3]);
            return 1;
        }
        if (fwrite(c.display, 1, sizeof(c.display), raw) != sizeof(c.display)) {
            fprintf(stderr, "Could not write output file '%s'\n", argv[3]);
            fclose(raw);
            return 1;
        }
        fclose(raw);
    }
    return 0;
}
