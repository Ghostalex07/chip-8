/* FX0A (getkey) test driver: runs 6-keypad.8o with menu poked to option 3,
 * pressing and releasing a key whenever the core is waiting (FX0A), and
 * snapshots the framebuffer the moment the success/error flag appears
 * (the ROM clears the screen between passes, so we must catch it). */
#include <stdio.h>
#include <stdlib.h>

#include "chip8.h"

/* flag-ok = A0 C0 80 at (30,9); flag-err = A0 40 A0 at (30,9). */
static int glyph_at(const uint8_t *fb, int x, int y, const uint8_t rows[3])
{
    for (int r = 0; r < 3; r++) {
        for (int b = 0; b < 3; b++) {
            int px = fb[(y + r) * CHIP8_DISPLAY_WIDTH + x + b];
            if (px != (int)((rows[r] >> (7 - b)) & 1)) {
                return 0;
            }
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom> [cycles] [fb_out]\n", argv[0]);
        return 1;
    }
    int cycles = 30000;
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
    c.memory[0x1FF] = 3; /* keypad menu: 3 = FX0A getkey test */

    int hold = 0;
    int presses = 0;
    int found_at = -1;
    int prev_wait = 0;

    for (int i = 0; i < cycles && found_at < 0; i++) {
        chip8_cycle(&c);

        if (c.waiting_for_key && !prev_wait) {
            printf("cycle %6d: WAIT start pc=0x%03X delay=%u\n", i, c.pc, c.delay_timer);
        } else if (!c.waiting_for_key && prev_wait) {
            printf("cycle %6d: WAIT done   pc=0x%03X delay=%u\n", i, c.pc, c.delay_timer);
        }
        prev_wait = c.waiting_for_key;

        if (c.waiting_for_key && hold == 0 && !c.keys[5]) {
            c.keys[5] = true; /* press key 5 */
            hold = 40;        /* long enough for delay=3 to reach 0 */
            presses++;
            printf("cycle %6d: press #%d\n", i, presses);
        }
        if (hold > 0) {
            hold--;
            if (hold == 0) {
                c.keys[5] = false; /* release: lets FX0A complete */
                printf("cycle %6d: release #%d\n", i, presses);
            }
        }

        if (i % 10 == 9) {
            chip8_frame_end(&c);
            chip8_tick_timers(&c);
        }

        static const uint8_t ok[3] = {0xA0, 0xC0, 0x80};
        static const uint8_t err[3] = {0xA0, 0x40, 0xA0};
        if (glyph_at(c.display, 30, 9, ok)) {
            found_at = i;
            printf("cycle %6d: flag-OK (v0=%02X v1=%02X vD=%u vE=%u delay=%u)\n",
                   i, c.v[0], c.v[1], c.v[0xD], c.v[0xE], c.delay_timer);
        } else if (glyph_at(c.display, 30, 9, err)) {
            fprintf(stderr, "cycle %6d: flag-ERR (v0=%02X v1=%02X vD=%u vE=%u delay=%u)\n",
                    i, c.v[0], c.v[1], c.v[0xD], c.v[0xE], c.delay_timer);
            found_at = -2;
            break;
        }
    }

    printf("presses=%d result_cycle=%d delay=%u\n", presses, found_at,
           c.delay_timer);

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
    return found_at >= 0 ? 0 : 1;
}
