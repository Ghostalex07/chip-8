/* End-to-end test of the bundled game games/catch.ch8 (source:
 * games/catch.asm, assembled by games/assemble.py).
 *
 * Plays the game headless with a fixed RNG seed, so the ball sequence is
 * deterministic:
 *   - the first ball lands on the paddle's column -> catching scores;
 *   - the first ball lands elsewhere -> a miss does not score;
 *   - holding the right/left key drives the paddle to the edges;
 *   - the score digits "00" are drawn at the top right.
 *
 * Takes no arguments. ROM path is relative to the repo root (the way
 * run_tests.sh invokes it). Exit status 0 = all checks passed. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "chip8.h"

#define ROM "games/catch.ch8"
#define PADDLE_X 24 /* games/catch.asm starts the paddle here */
#define KEY_LEFT 7  /* hex key 7 = QWERTY A */
#define KEY_RIGHT 9 /* hex key 9 = QWERTY D */
#define CAP 20000   /* generous cycle budget per drop */

static int failures = 0;

static void check(bool ok, const char *name)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", name);
    if (!ok) {
        failures++;
    }
}

/* Runs `cycles` cycles; pass a multiple of 10 so the frame_end/tick
 * cadence stays aligned inside a single call. */
static void run(Chip8 *c, int cycles)
{
    for (int i = 0; i < cycles; i++) {
        chip8_cycle(c);
        if (i % 10 == 9) {
            chip8_frame_end(c);
            chip8_tick_timers(c);
        }
    }
}

static void boot(Chip8 *c, unsigned seed, bool left, bool right)
{
    chip8_init(c);
    c->rng_state = seed;
    if (!chip8_load_rom(c, ROM)) {
        fprintf(stderr, "could not load %s (run from the repo root)\n", ROM);
        exit(2);
    }
    c->keys[KEY_LEFT] = left;
    c->keys[KEY_RIGHT] = right;
}

/* Runs until the game has spawned the first ball (V2 > 0, which means the
 * CXNN that picks V1 has already executed). */
static bool until_first_ball(Chip8 *c)
{
    for (int i = 0; i < 200; i++) {
        run(c, 10);
        if (c->v[2] > 0) {
            return true;
        }
    }
    return false;
}

/* Smallest seed whose first ball lands on (or, if same_as_paddle is false,
 * away from) the starting paddle column. */
static unsigned find_seed(bool same_as_paddle)
{
    for (unsigned seed = 1; seed < 10000; seed++) {
        Chip8 c;
        boot(&c, seed, false, false);
        if (!until_first_ball(&c)) {
            continue;
        }
        if ((c.v[1] == PADDLE_X) == same_as_paddle) {
            return seed;
        }
    }
    return 0;
}

static bool score_digits_lit(const Chip8 *c)
{
    for (int x = 56; x < 64; x++) {
        if (c->display[x] == 0) {
            return false;
        }
    }
    return true;
}

int main(void)
{
    /* 1. Catch: first ball on the paddle's column, no keys -> it scores. */
    unsigned seed = find_seed(true);
    check(seed != 0, "found a seed whose first ball is catchable");
    if (seed != 0) {
        Chip8 c;
        boot(&c, seed, false, false);
        check(until_first_ball(&c) && c.v[1] == PADDLE_X,
              "first ball spawns at x=24");
        int scored_at = -1;
        for (int i = 0; i < CAP && scored_at < 0; i += 100) {
            run(&c, 100);
            if (c.v[4] > 0) {
                scored_at = i;
            }
        }
        check(scored_at >= 0, "catch increments the score");
    }

    /* 2. Miss: first ball away from the column, no keys -> no score on
     * the first drop (the run stops the moment the ball respawns). */
    seed = find_seed(false);
    check(seed != 0, "found a seed whose first ball is uncatchable");
    if (seed != 0) {
        Chip8 c;
        boot(&c, seed, false, false);
        check(until_first_ball(&c) && c.v[1] != PADDLE_X,
              "first ball spawns away from x=24");
        bool resolved = false;
        for (int i = 0; i < CAP && !resolved; i += 100) {
            run(&c, 100);
            if (c.v[2] == 0) {
                resolved = true;
            }
        }
        check(resolved, "first drop finishes (ball respawns)");
        check(c.v[4] == 0, "miss does not score");
    }

    /* 3. Paddle: holding right/left walks it to the guarded edges. */
    {
        Chip8 c;
        boot(&c, 1, false, true);
        run(&c, 3000);
        check(c.v[0] == 56, "right key moves the paddle to x=56");

        boot(&c, 1, true, false);
        run(&c, 3000);
        check(c.v[0] == 0, "left key moves the paddle to x=0");
    }

    /* 4. Score: "00" is drawn at (56,0)/(60,0) once a frame is complete.
     * Ball y is >= 1 whenever it is drawn, so row 0 holds only digits. */
    seed = find_seed(false);
    if (seed != 0) {
        Chip8 c;
        boot(&c, seed, false, false);
        bool lit = false;
        for (int i = 0; i < 5000 && !lit; i += 10) {
            run(&c, 10);
            lit = score_digits_lit(&c);
        }
        check(lit, "score digits 00 drawn at top right");
    } else {
        check(false, "score digits 00 drawn at top right");
    }

    printf("%s: %d failure(s)\n", failures ? "FAIL" : "ok", failures);
    return failures == 0 ? 0 : 1;
}
