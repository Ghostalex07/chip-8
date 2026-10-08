#!/usr/bin/env bash
# Headless verification of the CHIP-8 core against the Timendus
# chip-8-test-suite (https://github.com/Timendus/chip8-test-suite).
#
# The suite's ROMs are GPL and are not bundled in this repository. Either
# point SUITE_BIN at the suite's bin/ directory or copy the .ch8 files
# into tests/roms/:
#
#   git clone https://github.com/Timendus/chip8-test-suite
#   SUITE_BIN=chip8-test-suite/bin tests/run_tests.sh
#
# 8-scrolling is skipped on purpose: it tests SCHIP features that are out
# of scope for this emulator.
set -u
cd "$(dirname "$0")/.."

ROM_DIR="${SUITE_BIN:-tests/roms}"
CC="${CC:-gcc}"
CFLAGS="-std=c99 -Wall -Wextra -pedantic -Isrc"
BUILD=tests/.build

failures=0
pass() { printf 'PASS  %s\n' "$1"; }
fail() { printf 'FAIL  %s\n' "$1"; failures=$((failures + 1)); }

for rom in 1-chip8-logo.ch8 2-ibm-logo.ch8 "3-corax+.ch8" 4-flags.ch8 \
           5-quirks.ch8 6-keypad.ch8 7-beep.ch8; do
    if [ ! -f "$ROM_DIR/$rom" ]; then
        echo "Missing '$ROM_DIR/$rom'." >&2
        echo "Get the suite (GPL) from https://github.com/Timendus/chip8-test-suite" >&2
        echo "and set SUITE_BIN or copy its bin/*.ch8 into tests/roms/." >&2
        exit 2
    fi
done

mkdir -p "$BUILD"

echo "== build harnesses =="
$CC $CFLAGS -o "$BUILD/dump" tests/dump.c src/chip8.c || exit 1
$CC $CFLAGS -o "$BUILD/fx0a" tests/test_fx0a.c src/chip8.c || exit 1

# run <rom> <cycles> <fb> [poke] [keys] -> 0 when stderr stays empty
run() {
    local rom="$1" cycles="$2" fb="$3" poke="${4-}" keys="${5-}"
    "$BUILD/dump" "$ROM_DIR/$rom" "$cycles" "$fb" $poke $keys \
        2>"$BUILD/stderr.txt" >/dev/null
    if [ -s "$BUILD/stderr.txt" ]; then
        echo "  stderr:"
        sort -u "$BUILD/stderr.txt" | head -5 | sed 's/^/    /'
        return 1
    fi
    return 0
}

echo "== tests =="

if run 1-chip8-logo.ch8 3000 "$BUILD/fb_logo.bin"; then pass "1-chip8-logo"; else fail "1-chip8-logo"; fi
if run 2-ibm-logo.ch8 3000 "$BUILD/fb_ibm.bin"; then pass "2-ibm-logo"; else fail "2-ibm-logo"; fi

if run "3-corax+.ch8" 6000 "$BUILD/fb_corax.bin"; then
    if python3 tests/check_corax.py "$BUILD/fb_corax.bin"; then pass "3-corax+"; else fail "3-corax+"; fi
else
    fail "3-corax+ (stderr)"
fi

if run 4-flags.ch8 10000 "$BUILD/fb_flags.bin"; then
    if python3 tests/check_flags.py "$BUILD/fb_flags.bin"; then pass "4-flags"; else fail "4-flags"; fi
else
    fail "4-flags (stderr)"
fi

# poke 0x1FF=1: the menu auto-selects the CHIP-8 platform
if run 5-quirks.ch8 60000 "$BUILD/fb_quirks.bin" 1; then
    if python3 tests/check_quirks.py "$BUILD/fb_quirks.bin"; then pass "5-quirks"; else fail "5-quirks"; fi
else
    fail "5-quirks (stderr)"
fi

# EX9E/EXA1: four runs (opcode x keys held) compared pairwise
kp_ok=1
run 6-keypad.ch8 60000 "$BUILD/k_e1.bin" 1 0    || kp_ok=0
run 6-keypad.ch8 60000 "$BUILD/k_e2.bin" 1 ffff || kp_ok=0
run 6-keypad.ch8 60000 "$BUILD/k_u1.bin" 2 0    || kp_ok=0
run 6-keypad.ch8 60000 "$BUILD/k_u2.bin" 2 ffff || kp_ok=0
if [ "$kp_ok" -eq 1 ] &&
   python3 tests/check_keypad.py EX9E "$BUILD/k_e1.bin" "$BUILD/k_e2.bin" \
       "$BUILD/k_u1.bin" "$BUILD/k_u2.bin"; then
    pass "6-keypad EX9E/EXA1"
else
    fail "6-keypad EX9E/EXA1"
fi

# FX0A: press-and-release driver with its own timing checks
if "$BUILD/fx0a" "$ROM_DIR/6-keypad.ch8" 30000 "$BUILD/k_fx0a.bin" \
        >"$BUILD/fx0a.log" 2>"$BUILD/fx0a.err" &&
   [ ! -s "$BUILD/fx0a.err" ] &&
   python3 tests/check_keypad.py FX0A "$BUILD/k_fx0a.bin"; then
    pass "6-keypad FX0A"
else
    sed 's/^/  /' "$BUILD/fx0a.log" | tail -4
    fail "6-keypad FX0A"
fi

# 7-beep: the sound icon must be on early and off later
if run 7-beep.ch8 50 "$BUILD/beep_on.bin" &&
   run 7-beep.ch8 150 "$BUILD/beep_off.bin"; then
    if python3 tests/check_beep.py "$BUILD/beep_on.bin" "$BUILD/beep_off.bin"; then
        pass "7-beep"
    else
        fail "7-beep"
    fi
else
    fail "7-beep (stderr)"
fi

echo "== result =="
if [ "$failures" -eq 0 ]; then
    echo "all tests passed"
else
    echo "$failures test(s) failed"
    exit 1
fi
