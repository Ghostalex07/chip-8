/* Tests for the error-handling conventions documented in AGENTS.md:
 * unknown opcodes and stack errors print to stderr in English and SKIP
 * the instruction (never abort); the PC wraps at 12 bits, including the
 * FX0A wait loop; each unique (pc, opcode) error prints only once;
 * chip8_load_rom rejects missing/empty/oversized files; timers floor
 * at zero.
 *
 * Takes no arguments. Captured stderr goes to tests/.build/errors.stderr.
 * Exit status 0 = all checks passed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chip8.h"

#define ERR_FILE "tests/.build/errors.stderr"

static int failures = 0;

static void check(bool ok, const char *name)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", name);
    if (!ok) {
        failures++;
    }
}

static int count_lines_containing(const char *needle)
{
    FILE *f = fopen(ERR_FILE, "r");
    if (f == NULL) {
        return -1;
    }
    char line[256];
    int count = 0;
    while (fgets(line, sizeof line, f) != NULL) {
        if (strstr(line, needle) != NULL) {
            count++;
        }
    }
    fclose(f);
    return count;
}

int main(void)
{
    if (freopen(ERR_FILE, "w", stderr) == NULL) {
        printf("FAIL  could not open %s for capture\n", ERR_FILE);
        return 1;
    }

    Chip8 c;

    /* Stack underflow: 00EE with an empty stack, hit 50 times through a
     * loop. The program must keep running (the instruction is skipped)
     * and the error must print exactly once (deduplicated). */
    chip8_init(&c);
    c.memory[0x200] = 0x00;
    c.memory[0x201] = 0xEE;
    c.memory[0x202] = 0x12;
    c.memory[0x203] = 0x00; /* JP 0x200 */
    for (int k = 0; k < 100; k++) {
        chip8_cycle(&c);
    }
    check(c.sp == 0, "stack underflow leaves the stack untouched");
    check(c.pc == 0x200, "program still running after underflow skips");

    /* Stack overflow: a self-call fills all 16 slots and then faults. */
    chip8_init(&c);
    c.memory[0x200] = 0x22;
    c.memory[0x201] = 0x04; /* CALL 0x204 */
    c.memory[0x204] = 0x22;
    c.memory[0x205] = 0x04; /* CALL 0x204: fills the stack */
    for (int k = 0; k < 100; k++) {
        chip8_cycle(&c);
    }
    check(c.sp == CHIP8_STACK_DEPTH, "stack overflow stops at depth 16");

    /* Undefined opcode: 5XY1 (N must be 0), executed in a loop. */
    chip8_init(&c);
    c.memory[0x200] = 0x51;
    c.memory[0x201] = 0x21;
    c.memory[0x202] = 0x12;
    c.memory[0x203] = 0x00; /* JP 0x200 */
    for (int k = 0; k < 100; k++) {
        chip8_cycle(&c);
    }
    check(c.pc == 0x200, "program still running after unknown-opcode skips");

    /* 12-bit PC: FX0A placed at 0xFFE must stay parked there instead of
     * underflowing the rewind to 0xFFFE and escaping the wait. */
    chip8_init(&c);
    c.memory[0xFFE] = 0xF0;
    c.memory[0xFFF] = 0x0A; /* FX0A V0 */
    c.pc = 0xFFE;
    chip8_cycle(&c);
    check(c.waiting_for_key && c.pc == 0xFFE,
          "FX0A at 0xFFE stays parked (12-bit PC wrap)");
    chip8_cycle(&c);
    check(c.pc == 0xFFE, "FX0A still parked after another cycle");
    c.keys[5] = true;
    chip8_cycle(&c);
    check(c.waiting_for_key && c.waiting_key == 5, "FX0A latches key 5");
    c.keys[5] = false;
    chip8_cycle(&c);
    check(!c.waiting_for_key && c.v[0] == 5, "FX0A completes on key release");
    check(c.pc == 0x000, "PC wraps past 0xFFF to 0x000");

    /* Timers tick down and floor at zero. */
    chip8_init(&c);
    c.delay_timer = 1;
    c.sound_timer = 2;
    chip8_tick_timers(&c);
    check(c.delay_timer == 0 && c.sound_timer == 1, "timers tick down");
    chip8_tick_timers(&c);
    chip8_tick_timers(&c);
    check(c.delay_timer == 0 && c.sound_timer == 0, "timers floor at zero");

    /* ROM loading failures: rejected, reported on stderr in English. */
    check(!chip8_load_rom(&c, "tests/.build/does-not-exist.ch8"),
          "missing ROM rejected");
    FILE *f = fopen("tests/.build/empty.ch8", "wb");
    if (f != NULL) {
        fclose(f);
    }
    check(!chip8_load_rom(&c, "tests/.build/empty.ch8"), "empty ROM rejected");
    f = fopen("tests/.build/big.ch8", "wb");
    if (f != NULL) {
        for (int k = 0; k <= CHIP8_MEMORY_SIZE - CHIP8_PROGRAM_START; k++) {
            fputc(0, f);
        }
        fclose(f);
    }
    check(!chip8_load_rom(&c, "tests/.build/big.ch8"), "oversized ROM rejected");

    /* The captured stderr itself: English messages, exactly one line per
     * message even though the errors above repeated dozens of times. */
    fflush(stderr);
    check(count_lines_containing("Stack underflow") == 1,
          "underflow message printed exactly once");
    check(count_lines_containing("Stack overflow") == 1,
          "overflow message printed exactly once");
    check(count_lines_containing("0x5121") == 1,
          "unknown-opcode message printed exactly once");
    check(count_lines_containing("Could not open ROM") == 1,
          "missing ROM reported in English");
    check(count_lines_containing("is empty") == 1,
          "empty ROM reported in English");
    check(count_lines_containing("too large") == 1,
          "oversized ROM reported in English");

    printf("%d check(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
