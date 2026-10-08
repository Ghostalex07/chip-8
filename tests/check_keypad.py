"""6-keypad verification.

EX9E/EXA1: four runs (opcode x keys-held) are compared pairwise. The runs
draw the same static keypad labels; the only difference is the 16 cursor
rects (6 rows of 0xFE at fixed coords), which appear when the test detects
a mismatch. Cursor rects:
  equivalence {E1, U2} must carry the cursors, {E2, U1} must be clean.

FX0A: expect the flag-ok glyph at (30,9) and no flag-err anywhere.
"""
import sys

W, H = 64, 32

CURSOR_POS = [
    (24, 23), (16, 2), (24, 2), (32, 2), (16, 9), (24, 9), (32, 9),
    (16, 16), (24, 16), (32, 16), (16, 23), (32, 23), (40, 2), (40, 9),
    (40, 16), (40, 23),
]
OK = [0xA0, 0xC0, 0x80]
ERR = [0xA0, 0x40, 0xA0]


def load(path):
    fb = open(path, "rb").read()
    assert len(fb) == W * H, path
    return fb


def rect(fb, x, y, w=8, h=6):
    return tuple(fb[(y + r) * W + x + c] for r in range(h) for c in range(w))


def rect_diff(a, b):
    return sum(1 for x, y in CURSOR_POS if rect(a, x, y) != rect(b, x, y))


def find_glyph(fb, pat):
    cols = [c for c in range(8) if any(b & (0x80 >> c) for b in pat)]
    hits = []
    for y in range(H - len(pat) + 1):
        for x in range(W - 2):
            if all(
                all(bool(fb[(y + r) * W + x + c] != 0) == bool(b & (0x80 >> c))
                    for c in cols)
                for r, b in enumerate(pat)
            ):
                hits.append((x, y))
    return hits


mode = sys.argv[1]

if mode in ("EX9E", "EXA1"):
    e1, e2, u1, u2 = (load(p) for p in sys.argv[2:6])
    # e*=keys held, u*=no keys... naming per plan: E1=EX9E-n keys, E2=EX9E-all, U1=EXA1-n keys, U2=EXA1-all
    checks = [
        ("EX9E: cursors only without keys", rect_diff(e1, u1), 16),
        ("EX9E all-held clean (=EXA1 no-keys)", rect_diff(e2, u1), 0),
        ("EXA1 cursors only with keys", rect_diff(u2, e2), 16),
        ("EXA1 no-keys equals EX9E all-held", rect_diff(u1, e2), 0),
        ("both-cursor screens identical", rect_diff(e1, u2), 0),
    ]
    ok = True
    for name, got, want in checks:
        good = got == want
        ok &= good
        print(f"{'OK ' if good else 'FAIL'} {name}: {got} (expected {want})")
    sys.exit(0 if ok else 1)

if mode == "FX0A":
    fb = load(sys.argv[2])
    oks = [h for h in find_glyph(fb, OK) if abs(h[0] - 30) <= 1 and h[1] == 9]
    errs = [h for h in find_glyph(fb, ERR) if abs(h[0] - 30) <= 1 and h[1] == 9]
    print(f"flag-ok at (30,9): {bool(oks)}; flag-err at (30,9): {bool(errs)}")
    sys.exit(0 if oks and not errs else 1)
