# AGENTS.md

## Stack

C99 + SDL2, plain Makefile. The machine core (`src/chip8.c`/`.h`) must
stay SDL-free so it can run headless in test harnesses.

## Make targets

- `make all` — builds `./chip8` (`-std=c99 -Wall -Wextra -pedantic -O2`)
- `make clean` — removes objects and the binary

## Conventions

- Every change ends with `make clean && make` finishing with **zero
  warnings**.
- Errors go to stderr in English. Bad ROMs return false from
  `chip8_load_rom`; stack overflow/underflow and unknown opcodes print a
  message and skip the instruction instead of aborting.
- Opcode comments state the spec behavior and every quirk effect in
  place; quirks default to original COSMAC VIP (`chip8_init`).
- All five quirks live in `Chip8Quirks` and are overridable from the CLI
  (`--no-vblank`, `--no-mem-inc`, `--no-logic-vf`, `--no-shift-vy`,
  `--jump-vx` in `main.c`). The host must call `chip8_frame_end()` once
  per frame (releases a DXYN parked by `display_waits_for_vblank`) or
  the core stalls on the first draw.
- 0NNN is a silent no-op; FX1E does not touch VF; FX0A completes on key
  release. These are suite-driven decisions.
- Verification: `SUITE_BIN=<suite>/bin tests/run_tests.sh` runs the
  Timendus chip-8-test-suite headless (framebuffer dumps + glyph-marker
  checkers in `tests/`). Suite ROMs are GPL and not bundled; binaries
  build into `tests/.build/` (gitignored).
