#ifndef CHIP8_H
#define CHIP8_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Fixed sizes of the CHIP-8 machine.
 */
#define CHIP8_MEMORY_SIZE 4096
#define CHIP8_PROGRAM_START 0x200
#define CHIP8_FONT_START 0x050
#define CHIP8_FONT_SIZE 80
#define CHIP8_DISPLAY_WIDTH 64
#define CHIP8_DISPLAY_HEIGHT 32
#define CHIP8_REGISTER_COUNT 16
#define CHIP8_STACK_DEPTH 16
#define CHIP8_KEY_COUNT 16

/*
 * Quirks: the CHIP-8 specification is ambiguous on several opcodes and
 * modern emulators disagree on the right behavior. They are grouped here
 * in a configuration struct so the behavior can be switched without
 * touching the opcode code. The defaults are the original COSMAC VIP
 * behavior:
 *
 * - shift_copies_vy: on 8XY6/8XYE, if true VY is copied into VX before
 *   shifting (COSMAC VIP); if false VX is shifted in place (SCHIP/modern).
 * - memory_increments_i: on FX55/FX65, if true I ends up pointing at
 *   I + X + 1 after copying the registers (COSMAC VIP); if false I is left
 *   unchanged.
 * - logic_resets_vf: on 8XY1/8XY2/8XY3 (OR/AND/XOR), if true VF is reset
 *   to 0 as a side effect (COSMAC VIP); if false VF is not touched.
 * - jump_uses_v0: on BNNN, if true the jump goes to NNN + V0 (COSMAC VIP);
 *   if false it goes to NNN + VX (SCHIP), where X is the middle nibble.
 * - display_waits_for_vblank: on DXYN, if true each sprite draw blocks
 *   the CPU until the end of the current frame (COSMAC VIP: 30 sprites
 *   cost 30 frames); if false sprites draw instantly (SCHIP/modern).
 *   The host must call chip8_frame_end() once per frame to release the
 *   CPU.
 */
typedef struct {
    bool shift_copies_vy;
    bool memory_increments_i;
    bool logic_resets_vf;
    bool jump_uses_v0;
    bool display_waits_for_vblank;
} Chip8Quirks;

/*
 * Full state of the machine. It does not depend on SDL, so it can be
 * exercised and tested on its own.
 */
typedef struct {
    uint8_t memory[CHIP8_MEMORY_SIZE];
    uint8_t v[CHIP8_REGISTER_COUNT]; /* V0–VF */
    uint16_t i;                      /* index register (12 effective bits) */
    uint16_t pc;                     /* program counter */
    uint16_t stack[CHIP8_STACK_DEPTH];
    uint8_t sp;                      /* number of used stack slots */
    uint8_t delay_timer;
    uint8_t sound_timer;
    uint8_t display[CHIP8_DISPLAY_WIDTH * CHIP8_DISPLAY_HEIGHT]; /* 0 = off, 1 = on */
    bool keys[CHIP8_KEY_COUNT];      /* current state of the hex keypad */
    bool waiting_for_key;            /* true while an FX0A wait is active */
    int waiting_key;                 /* latched key for FX0A, -1 until pressed */
    bool waiting_for_frame;          /* DXYN drawn; CPU blocked until frame end */
    Chip8Quirks quirks;
} Chip8;

/* Sets up the machine: zeroed memory, font at 0x050, PC at 0x200. */
void chip8_init(Chip8 *chip8);

/* Loads a ROM at 0x200. Returns false and prints an error to stderr if the
 * ROM does not exist, is empty, or does not fit in memory. */
bool chip8_load_rom(Chip8 *chip8, const char *path);

/* Executes one instruction (fetch, decode, execute). */
void chip8_cycle(Chip8 *chip8);

/* Decrements the delay and sound timers (call at 60 Hz). */
void chip8_tick_timers(Chip8 *chip8);

/* Releases a CPU blocked by the display_waits_for_vblank quirk (call once
 * per frame, after the frame has been presented). */
void chip8_frame_end(Chip8 *chip8);

#endif /* CHIP8_H */
