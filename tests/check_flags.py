"""Count flag-ok / flag-err checkmarks in the 4-flags framebuffer.

Mark columns are fixed: col1 starts at x=5, col2 at 27, col3 at 49, each
mark 4 px apart (3 px glyph). Section text at x<5 causes false positives,
so only valid mark columns are counted.
"""
import sys

W, H = 64, 32

# flag-ok reads 3 rows: its two bytes plus the first byte of the next label
OK = [0b10100000, 0b11000000, 0b10000000]
ERR = [0b10100000, 0b01000000, 0b10100000]

VALID_X = {5, 9, 13, 27, 31, 35, 39, 49, 53, 57, 61}
MARK_ROWS = {1, 6, 11, 17, 22, 28}

fb = open(sys.argv[1], "rb").read()
assert len(fb) == W * H


def find(pat):
    hits = []
    cols = [c for c in range(8) if any(b & (0x80 >> c) for b in pat)]
    for y in range(H - len(pat) + 1):
        for x in range(W - 2):
            if all(
                all(bool(fb[(y + r) * W + x + c] != 0) == bool(b & (0x80 >> c))
                    for c in cols)
                for r, b in enumerate(pat)
            ):
                hits.append((x, y))
    return hits


oks = [h for h in find(OK) if h[0] in VALID_X and h[1] in MARK_ROWS]
errs = [h for h in find(ERR) if h[0] in VALID_X and h[1] in MARK_ROWS]
print(f"ok marks:  {len(oks)}")
print(f"err marks: {len(errs)} at {sorted(errs)}")
sys.exit(0 if len(oks) == 47 and len(errs) == 0 else 1)
