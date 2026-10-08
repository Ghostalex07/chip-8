#include "chip8.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Font: the 16 hexadecimal digits, 5 bytes each (80 bytes total). Each byte
 * is one row of the sprite, with the pixels in the 4 high bits. Loaded into
 * memory at startup, at 0x050–0x09F.
 */
static const uint8_t chip8_font[CHIP8_FONT_SIZE] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, /* 0 */
    0x20, 0x60, 0x20, 0x20, 0x70, /* 1 */
    0xF0, 0x10, 0xF0, 0x80, 0xF0, /* 2 */
    0xF0, 0x10, 0xF0, 0x10, 0xF0, /* 3 */
    0x90, 0x90, 0xF0, 0x10, 0x10, /* 4 */
    0xF0, 0x80, 0xF0, 0x10, 0xF0, /* 5 */
    0xF0, 0x80, 0xF0, 0x90, 0xF0, /* 6 */
    0xF0, 0x10, 0x20, 0x40, 0x40, /* 7 */
    0xF0, 0x90, 0xF0, 0x90, 0xF0, /* 8 */
    0xF0, 0x90, 0xF0, 0x10, 0xF0, /* 9 */
    0xF0, 0x90, 0xF0, 0x90, 0x90, /* A */
    0xE0, 0x90, 0xE0, 0x90, 0xE0, /* B */
    0xF0, 0x80, 0x80, 0x80, 0xF0, /* C */
    0xE0, 0x90, 0x90, 0x90, 0xE0, /* D */
    0xF0, 0x80, 0xF0, 0x80, 0xF0, /* E */
    0xF0, 0x80, 0xF0, 0x80, 0x80, /* F */
};

void chip8_init(Chip8 *chip8)
{
    memset(chip8, 0, sizeof(*chip8));

    memcpy(&chip8->memory[CHIP8_FONT_START], chip8_font, CHIP8_FONT_SIZE);

    chip8->pc = CHIP8_PROGRAM_START;

    /* Seed the random number generator used by CXNN. */
    srand((unsigned int)time(NULL));

    /* Default behavior: original COSMAC VIP. */
    chip8->quirks.shift_copies_vy = true;
    chip8->quirks.memory_increments_i = true;
    chip8->quirks.logic_resets_vf = true;
    chip8->quirks.jump_uses_v0 = true;
    chip8->quirks.display_waits_for_vblank = true;
}

bool chip8_load_rom(Chip8 *chip8, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open ROM '%s': %s\n", path, strerror(errno));
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "Could not read ROM '%s': %s\n", path, strerror(errno));
        fclose(file);
        return false;
    }

    long size = ftell(file);
    if (size < 0) {
        fprintf(stderr, "Could not measure ROM '%s': %s\n", path, strerror(errno));
        fclose(file);
        return false;
    }

    if (size == 0) {
        fprintf(stderr, "ROM '%s' is empty\n", path);
        fclose(file);
        return false;
    }

    if (size > (long)(CHIP8_MEMORY_SIZE - CHIP8_PROGRAM_START)) {
        fprintf(stderr,
                "ROM '%s' is too large: %ld bytes (max %d bytes)\n",
                path, size, CHIP8_MEMORY_SIZE - CHIP8_PROGRAM_START);
        fclose(file);
        return false;
    }

    rewind(file);
    size_t read = fread(&chip8->memory[CHIP8_PROGRAM_START], 1, (size_t)size, file);
    fclose(file);

    if (read != (size_t)size) {
        fprintf(stderr, "Could not read ROM '%s': expected %ld bytes, got %zu\n",
                path, size, read);
        return false;
    }

    return true;
}

void chip8_cycle(Chip8 *chip8)
{
    /* Fetch: instructions are 2 bytes, big-endian. Addresses are 12 bits on
     * the real machine, so the PC wraps around the 4 KB of memory. */
    uint16_t pc = (uint16_t)(chip8->pc & 0x0FFF);
    uint16_t opcode = (uint16_t)((chip8->memory[pc] << 8) |
                                 chip8->memory[(uint16_t)((pc + 1) & 0x0FFF)]);
    chip8->pc = (uint16_t)((pc + 2) & 0x0FFF);

    /* Decode: the usual fields of an XYN/NN/NNN instruction. */
    uint16_t nnn = (uint16_t)(opcode & 0x0FFF);       /* address: 12 bits */
    uint8_t nn = (uint8_t)(opcode & 0x00FF);          /* low byte: 8 bits */
    uint8_t n = (uint8_t)(opcode & 0x000F);           /* low nibble: 4 bits */
    uint8_t x = (uint8_t)((opcode >> 8) & 0x000F);    /* nibble of register VX */
    uint8_t y = (uint8_t)((opcode >> 4) & 0x000F);    /* nibble of register VY */

    switch ((opcode >> 12) & 0x000F) {
    case 0x0:
        if (opcode == 0x00E0) {
            /* 00E0 - CLS: clear the screen (all pixels to 0). No VF effect. */
            memset(chip8->display, 0, sizeof(chip8->display));
        } else if (opcode == 0x00EE) {
            /* 00EE - RET: return from subroutine. Pops the stack and jumps
             * to the saved address (the instruction after the CALL).
             * No VF effect. On underflow, the error is reported and the
             * instruction is skipped. */
            if (chip8->sp == 0) {
                fprintf(stderr, "Stack underflow: 00EE executed with no active subroutines\n");
            } else {
                chip8->sp--;
                chip8->pc = chip8->stack[chip8->sp];
            }
        } else {
            /* 0NNN - SYS addr: ignored on CHIP-8 (historical RCA 1802 call).
             * Treated as a silent no-op per spec. */
        }
        break;

    case 0x1:
        /* 1NNN - JP addr: PC = NNN. No VF effect. */
        chip8->pc = nnn;
        break;

    case 0x2:
        /* 2NNN - CALL addr: push the return address (already past this
         * instruction) on the stack and jump to NNN. No VF effect.
         * On overflow, the error is reported and the call is skipped. */
        if (chip8->sp >= CHIP8_STACK_DEPTH) {
            fprintf(stderr, "Stack overflow: CALL 0x%03X with %d active subroutines\n",
                    nnn, CHIP8_STACK_DEPTH);
        } else {
            chip8->stack[chip8->sp] = chip8->pc;
            chip8->sp = (uint8_t)(chip8->sp + 1);
            chip8->pc = nnn;
        }
        break;

    case 0x3:
        /* 3XNN - SE Vx, byte: skip the next instruction if VX == NN.
         * No VF effect. */
        if (chip8->v[x] == nn) {
            chip8->pc = (uint16_t)(chip8->pc + 2);
        }
        break;

    case 0x4:
        /* 4XNN - SNE Vx, byte: skip the next instruction if VX != NN.
         * No VF effect. */
        if (chip8->v[x] != nn) {
            chip8->pc = (uint16_t)(chip8->pc + 2);
        }
        break;

    case 0x5:
        /* 5XY0 - SE Vx, Vy: skip the next instruction if VX == VY.
         * No VF effect. N must be 0; 5XYN is not a defined opcode. */
        if (n != 0) {
            fprintf(stderr, "Opcode not implemented: 0x%04X\n", opcode);
            break;
        }
        if (chip8->v[x] == chip8->v[y]) {
            chip8->pc = (uint16_t)(chip8->pc + 2);
        }
        break;

    case 0x6:
        /* 6XNN - LD Vx, byte: VX = NN. No VF effect. */
        chip8->v[x] = nn;
        break;

    case 0x7:
        /* 7XNN - ADD Vx, byte: VX = VX + NN (no carry, VF is not touched). */
        chip8->v[x] = (uint8_t)(chip8->v[x] + nn);
        break;

    case 0x8: {
        /* Arithmetic and logic group: the operands are read into locals
         * first, because VF may be one of them (X or Y can be F) and the
         * flag is always written last. */
        uint8_t vx = chip8->v[x];
        uint8_t vy = chip8->v[y];

        switch (n) {
        case 0x0:
            /* 8XY0 - LD Vx, Vy: VX = VY. No VF effect. */
            chip8->v[x] = vy;
            break;

        case 0x1:
            /* 8XY1 - OR Vx, Vy: VX = VX | VY. Quirk logic_resets_vf:
             * the original COSMAC VIP resets VF to 0 afterwards. */
            chip8->v[x] = (uint8_t)(vx | vy);
            if (chip8->quirks.logic_resets_vf) {
                chip8->v[0xF] = 0;
            }
            break;

        case 0x2:
            /* 8XY2 - AND Vx, Vy: VX = VX & VY. Quirk logic_resets_vf:
             * the original COSMAC VIP resets VF to 0 afterwards. */
            chip8->v[x] = (uint8_t)(vx & vy);
            if (chip8->quirks.logic_resets_vf) {
                chip8->v[0xF] = 0;
            }
            break;

        case 0x3:
            /* 8XY3 - XOR Vx, Vy: VX = VX ^ VY. Quirk logic_resets_vf:
             * the original COSMAC VIP resets VF to 0 afterwards. */
            chip8->v[x] = (uint8_t)(vx ^ vy);
            if (chip8->quirks.logic_resets_vf) {
                chip8->v[0xF] = 0;
            }
            break;

        case 0x4: {
            /* 8XY4 - ADD Vx, Vy: VX = VX + VY. VF = 1 on carry (result
             * > 255), 0 otherwise. The flag is computed from the original
             * operands and written after VX (if X == F, the flag wins). */
            unsigned int sum = (unsigned int)vx + vy;
            chip8->v[x] = (uint8_t)sum;
            chip8->v[0xF] = (sum > 0xFF) ? 1 : 0;
            break;
        }

        case 0x5: {
            /* 8XY5 - SUB Vx, Vy: VX = VX - VY. VF = 1 when there is no
             * borrow (VX >= VY), 0 otherwise. Flag written after VX. */
            bool no_borrow = vx >= vy;
            chip8->v[x] = (uint8_t)(vx - vy);
            chip8->v[0xF] = no_borrow ? 1 : 0;
            break;
        }

        case 0x6: {
            /* 8XY6 - SHR Vx: shift right by 1. Quirk shift_copies_vy:
             * VY is copied into VX first (COSMAC VIP); otherwise VX is
             * shifted in place (SCHIP). VF = the bit shifted out, written
             * after VX (if X == F, the flag wins). */
            uint8_t src = chip8->quirks.shift_copies_vy ? vy : vx;
            chip8->v[x] = (uint8_t)(src >> 1);
            chip8->v[0xF] = (uint8_t)(src & 0x01);
            break;
        }

        case 0x7: {
            /* 8XY7 - SUBN Vx, Vy: VX = VY - VX. VF = 1 when there is no
             * borrow (VY >= VX), 0 otherwise. Flag written after VX. */
            bool no_borrow = vy >= vx;
            chip8->v[x] = (uint8_t)(vy - vx);
            chip8->v[0xF] = no_borrow ? 1 : 0;
            break;
        }

        case 0xE: {
            /* 8XYE - SHL Vx: shift left by 1. Quirk shift_copies_vy:
             * VY is copied into VX first (COSMAC VIP); otherwise VX is
             * shifted in place (SCHIP). VF = the bit shifted out, written
             * after VX (if X == F, the flag wins). */
            uint8_t src = chip8->quirks.shift_copies_vy ? vy : vx;
            chip8->v[x] = (uint8_t)(src << 1);
            chip8->v[0xF] = (uint8_t)((src >> 7) & 0x01);
            break;
        }

        default:
            fprintf(stderr, "Opcode not implemented: 0x%04X\n", opcode);
            break;
        }
        break;
    }

    case 0x9:
        /* 9XY0 - SNE Vx, Vy: skip the next instruction if VX != VY.
         * No VF effect. N must be 0; 9XYN is not a defined opcode. */
        if (n != 0) {
            fprintf(stderr, "Opcode not implemented: 0x%04X\n", opcode);
            break;
        }
        if (chip8->v[x] != chip8->v[y]) {
            chip8->pc = (uint16_t)(chip8->pc + 2);
        }
        break;

    case 0xA:
        /* ANNN - LD I, addr: I = NNN. No VF effect. */
        chip8->i = nnn;
        break;

    case 0xB:
        /* BNNN - JP V0, addr: jump to NNN + V0 (quirk jump_uses_v0, the
         * COSMAC VIP behavior). With the quirk disabled (SCHIP), the
         * offset is VX instead, where X is the middle nibble of the
         * opcode. No VF effect. */
        if (chip8->quirks.jump_uses_v0) {
            chip8->pc = (uint16_t)(nnn + chip8->v[0]);
        } else {
            chip8->pc = (uint16_t)(nnn + chip8->v[x]);
        }
        break;

    case 0xC:
        /* CXNN - RND Vx, byte: VX = (random byte) & NN. The mask limits
         * the result to NN's bits. No VF effect. */
        chip8->v[x] = (uint8_t)(rand() & nn);
        break;

    case 0xD:
        /* DXYN - DRW Vx, Vy, nibble: draw an N-byte sprite at (VX, VY).
         * The starting coordinates wrap (modulo 64/32); pixels leaving the
         * screen are clipped. It is an XOR draw: a pixel that was on turns
         * off. VF = 1 if any pixel was turned off (collision), 0 otherwise.
         * VF is written last, in case X or Y is F (the flag must not
         * interfere with the drawing).
         * Quirk display_waits_for_vblank (COSMAC VIP): after drawing, the
         * CPU is blocked on this same instruction until the host signals
         * the end of the frame with chip8_frame_end(), so every sprite
         * costs one frame. */
        if (chip8->waiting_for_frame) {
            /* Already drawn: stay parked on this instruction. */
            chip8->pc = (uint16_t)((chip8->pc - 2) & 0x0FFF);
            break;
        }
        {
            int start_x = chip8->v[x] % CHIP8_DISPLAY_WIDTH;
            int start_y = chip8->v[y] % CHIP8_DISPLAY_HEIGHT;
            bool collision = false;

            for (int row = 0; row < n; row++) {
                uint16_t address = (uint16_t)(chip8->i + row);
                if (address >= CHIP8_MEMORY_SIZE) {
                    break;
                }
                uint8_t sprite = chip8->memory[address];
                int py = start_y + row;
                if (py >= CHIP8_DISPLAY_HEIGHT) {
                    break; /* sprite leaves through the bottom: clipped */
                }
                for (int bit = 0; bit < 8; bit++) {
                    if ((sprite & (0x80 >> bit)) == 0) {
                        continue;
                    }
                    int px = start_x + bit;
                    if (px >= CHIP8_DISPLAY_WIDTH) {
                        continue; /* sprite leaves through the right edge */
                    }
                    uint8_t *pixel = &chip8->display[py * CHIP8_DISPLAY_WIDTH + px];
                    if (*pixel == 1) {
                        collision = true;
                    }
                    *pixel ^= 1;
                }
            }

            chip8->v[0xF] = collision ? 1 : 0;
        }

        if (chip8->quirks.display_waits_for_vblank) {
            chip8->waiting_for_frame = true;
            chip8->pc = (uint16_t)((chip8->pc - 2) & 0x0FFF);
        }
        break;

    case 0xE:
        /* EX9E/EXA1 - key skips: skip the next instruction depending on
         * whether the key numbered by VX is currently pressed. VX is
         * masked to the 16-key keypad. No VF effect. */
        switch (nn) {
        case 0x9E:
            /* EX9E - SKP Vx: skip if the key is pressed. */
            if (chip8->keys[chip8->v[x] & 0x0F]) {
                chip8->pc = (uint16_t)(chip8->pc + 2);
            }
            break;

        case 0xA1:
            /* EXA1 - SKNP Vx: skip if the key is not pressed. */
            if (!chip8->keys[chip8->v[x] & 0x0F]) {
                chip8->pc = (uint16_t)(chip8->pc + 2);
            }
            break;

        default:
            fprintf(stderr, "Opcode not implemented: 0x%04X\n", opcode);
            break;
        }
        break;

    case 0xF:
        switch (nn) {
        case 0x0A:
            /* FX0A - LD Vx, k: wait for a key press and then for its
             * release, and only then VX = key. The PC stays on this
             * instruction while waiting (the CPU is effectively halted);
             * the delay and sound timers keep ticking. The first key that
             * goes down is latched and must be the one released. */
            if (!chip8->waiting_for_key) {
                chip8->waiting_for_key = true;
                chip8->waiting_key = -1;
            }
            if (chip8->waiting_key < 0) {
                for (int k = 0; k < CHIP8_KEY_COUNT; k++) {
                    if (chip8->keys[k]) {
                        chip8->waiting_key = k;
                        break;
                    }
                }
            }
            if (chip8->waiting_key >= 0 && !chip8->keys[chip8->waiting_key]) {
                chip8->v[x] = (uint8_t)chip8->waiting_key;
                chip8->waiting_for_key = false;
            } else {
                chip8->pc = (uint16_t)(chip8->pc - 2);
            }
            break;

        case 0x07:
            /* FX07 - LD Vx, delay: VX = the delay timer. No VF effect. */
            chip8->v[x] = chip8->delay_timer;
            break;

        case 0x15:
            /* FX15 - LD delay, Vx: the delay timer = VX. No VF effect. */
            chip8->delay_timer = chip8->v[x];
            break;

        case 0x18:
            /* FX18 - LD sound, Vx: the sound timer = VX (the speaker
             * beeps while it counts down). No VF effect. */
            chip8->sound_timer = chip8->v[x];
            break;

        case 0x1E:
            /* FX1E - ADD I, Vx: I = I + VX. No VF effect (the COSMAC VIP
             * does not set the overflow flag here, and no quirk applies). */
            chip8->i = (uint16_t)(chip8->i + chip8->v[x]);
            break;

        case 0x29:
            /* FX29 - LD F, Vx: I = the address of the font sprite for the
             * hex digit in VX (5 bytes per digit at 0x050). No VF effect. */
            chip8->i = (uint16_t)(CHIP8_FONT_START + (chip8->v[x] & 0x0F) * 5);
            break;

        case 0x33:
            /* FX33 - LD B, Vx: store the binary-coded decimal of VX at
             * I, I+1 (hundreds/tens) and I+2 (ones). I is unchanged.
             * No VF effect. Bytes outside memory are skipped. */
            {
                uint8_t value = chip8->v[x];
                uint8_t digits[3] = {
                    (uint8_t)(value / 100),
                    (uint8_t)((value / 10) % 10),
                    (uint8_t)(value % 10),
                };
                for (int k = 0; k < 3; k++) {
                    unsigned int address = (unsigned int)chip8->i + (unsigned int)k;
                    if (address < CHIP8_MEMORY_SIZE) {
                        chip8->memory[address] = digits[k];
                    }
                }
            }
            break;

        case 0x55:
            /* FX55 - LD [I], Vx: store V0..VX starting at I. Quirk
             * memory_increments_i: afterwards I = I + X + 1 (COSMAC VIP);
             * otherwise I is left unchanged. No VF effect. */
            for (int k = 0; k <= x; k++) {
                unsigned int address = (unsigned int)chip8->i + (unsigned int)k;
                if (address < CHIP8_MEMORY_SIZE) {
                    chip8->memory[address] = chip8->v[k];
                }
            }
            if (chip8->quirks.memory_increments_i) {
                chip8->i = (uint16_t)(chip8->i + x + 1);
            }
            break;

        case 0x65:
            /* FX65 - LD Vx, [I]: load V0..VX from I. Quirk
             * memory_increments_i: afterwards I = I + X + 1 (COSMAC VIP);
             * otherwise I is left unchanged. No VF effect. */
            for (int k = 0; k <= x; k++) {
                unsigned int address = (unsigned int)chip8->i + (unsigned int)k;
                if (address < CHIP8_MEMORY_SIZE) {
                    chip8->v[k] = chip8->memory[address];
                }
            }
            if (chip8->quirks.memory_increments_i) {
                chip8->i = (uint16_t)(chip8->i + x + 1);
            }
            break;

        default:
            fprintf(stderr, "Opcode not implemented: 0x%04X\n", opcode);
            break;
        }
        break;

    default:
        fprintf(stderr, "Opcode not implemented: 0x%04X\n", opcode);
        break;
    }
}

void chip8_tick_timers(Chip8 *chip8)
{
    if (chip8->delay_timer > 0) {
        chip8->delay_timer--;
    }
    if (chip8->sound_timer > 0) {
        chip8->sound_timer--;
    }
}

void chip8_frame_end(Chip8 *chip8)
{
    if (chip8->waiting_for_frame) {
        chip8->waiting_for_frame = false;
        /* The CPU is parked on the DXYN that is waiting: step past it. */
        chip8->pc = (uint16_t)((chip8->pc + 2) & 0x0FFF);
    }
}
