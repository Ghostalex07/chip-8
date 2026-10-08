#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "audio.h"
#include "chip8.h"
#include "display.h"
#include "input.h"

#define DEFAULT_CYCLES 10 /* instructions per frame → ~600 Hz */
#define DEFAULT_SCALE 10  /* 640×320 window */
#define TARGET_FPS 60
#define TARGET_FRAME_MS (1000.0 / TARGET_FPS)

static void print_usage(const char *program)
{
    fprintf(stderr, "Usage: %s <rom_path> [--cycles N] [--scale N] [quirk flags]\n", program);
    fprintf(stderr, "  --cycles N  instructions per frame (default %d)\n", DEFAULT_CYCLES);
    fprintf(stderr, "  --scale N   window pixels per CHIP-8 pixel (default %d)\n", DEFAULT_SCALE);
    fprintf(stderr, "  Quirk flags (default = original COSMAC VIP):\n");
    fprintf(stderr, "    --no-vblank    sprites don't wait for vertical blanking\n");
    fprintf(stderr, "    --no-mem-inc   FX55/FX65 don't increment I\n");
    fprintf(stderr, "    --no-logic-vf  OR/AND/XOR don't reset VF\n");
    fprintf(stderr, "    --no-shift-vy  8XY6/8XYE shift VX in place instead of copying VY\n");
    fprintf(stderr, "    --jump-vx      BNNN jumps to NNN+VX instead of NNN+V0\n");
}

static bool parse_positive_int(const char *flag, const char *text, int max, int *out)
{
    char *end;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value <= 0 || value > max) {
        fprintf(stderr, "Invalid value for %s: '%s'\n", flag, text);
        return false;
    }
    *out = (int)value;
    return true;
}

static bool parse_arguments(int argc, char **argv, const char **rom_path, int *cycles,
                            int *scale, Chip8Quirks *quirks)
{
    *rom_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cycles") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing value for --cycles\n");
                return false;
            }
            i++;
            if (!parse_positive_int("--cycles", argv[i], 100000, cycles)) {
                return false;
            }
        } else if (strcmp(argv[i], "--scale") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing value for --scale\n");
                return false;
            }
            i++;
            if (!parse_positive_int("--scale", argv[i],
                                    INT_MAX / CHIP8_DISPLAY_WIDTH, scale)) {
                return false;
            }
        } else if (strcmp(argv[i], "--no-vblank") == 0) {
            quirks->display_waits_for_vblank = false;
        } else if (strcmp(argv[i], "--no-mem-inc") == 0) {
            quirks->memory_increments_i = false;
        } else if (strcmp(argv[i], "--no-logic-vf") == 0) {
            quirks->logic_resets_vf = false;
        } else if (strcmp(argv[i], "--no-shift-vy") == 0) {
            quirks->shift_copies_vy = false;
        } else if (strcmp(argv[i], "--jump-vx") == 0) {
            quirks->jump_uses_v0 = false;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return false;
        } else if (*rom_path == NULL) {
            *rom_path = argv[i];
        } else {
            fprintf(stderr, "Unexpected argument: %s\n", argv[i]);
            return false;
        }
    }

    if (*rom_path == NULL) {
        fprintf(stderr, "Missing ROM path\n");
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    /* The CHIP-8 core does not depend on SDL: it can be initialized and
     * tested on its own. It is initialized first so the quirk flags can
     * override the COSMAC VIP defaults. */
    Chip8 chip8;
    chip8_init(&chip8);

    const char *rom_path;
    int cycles = DEFAULT_CYCLES;
    int scale = DEFAULT_SCALE;

    if (!parse_arguments(argc, argv, &rom_path, &cycles, &scale, &chip8.quirks)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (!chip8_load_rom(&chip8, rom_path)) {
        return EXIT_FAILURE;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    Display display;
    if (!display_init(&display, scale)) {
        SDL_Quit();
        return EXIT_FAILURE;
    }

    Input input;
    input_init(&input);

    Audio audio;
    if (!audio_init(&audio)) {
        display_destroy(&display);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    const Uint64 performance_frequency = SDL_GetPerformanceFrequency();
    double delay_credit = 0.0; /* sleep milliseconds owed to the pacer */
    Uint64 previous_frame = 0;
    bool have_previous_frame = false;

    while (!input.quit) {
        /* Fixed 60 FPS pace: the timers and the emulated CPU speed depend
         * on this loop. The sleep budget is accumulated as an error term
         * instead of truncating a fractional millisecond every frame, so
         * the loop runs at exactly 60 Hz (the old way averaged ~60.7). */
        const Uint64 now = SDL_GetPerformanceCounter();
        if (have_previous_frame) {
            const double period_ms =
                (double)(now - previous_frame) * 1000.0 /
                (double)performance_frequency;
            delay_credit += TARGET_FRAME_MS - period_ms;
            if (delay_credit > 4.0 * TARGET_FRAME_MS) {
                delay_credit = 4.0 * TARGET_FRAME_MS; /* anti-spiral guard */
            } else if (delay_credit < -TARGET_FRAME_MS) {
                delay_credit = 0.0;
            }
        }
        previous_frame = now;
        have_previous_frame = true;

        input_poll(&input);
        if (input.quit) {
            break;
        }
        memcpy(chip8.keys, input.keys, sizeof(chip8.keys));

        for (int i = 0; i < cycles; i++) {
            chip8_cycle(&chip8);
        }
        chip8_tick_timers(&chip8);

        audio_set_beeping(&audio, chip8.sound_timer > 0);
        display_render(&display, chip8.display);
        chip8_frame_end(&chip8); /* releases a CPU waiting for v-blank */

        if (delay_credit >= 1.0) {
            SDL_Delay((Uint32)delay_credit);
        }
    }

    audio_close(&audio);
    display_destroy(&display);
    SDL_Quit();
    return EXIT_SUCCESS;
}
