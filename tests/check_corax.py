"""Locate image-ok / image-no markers in the corax+ framebuffer.

The marker glyphs only use their 3 left columns; the remaining 5 columns of
the 8x4 sprite window overlap neighbouring graphics, so only the active
columns are compared.
"""
import sys

W, H = 64, 32

OK = [0b00000000, 0b10100000, 0b11000000, 0b10000000]
NO = [0b00000000, 0b10100000, 0b01000000, 0b10100000]

fb = open(sys.argv[1], "rb").read()
assert len(fb) == W * H


def active_cols(pat):
    cols = []
    for c in range(8):
        if any(byte & (0x80 >> c) for byte in pat):
            cols.append(c)
    return cols


def find(pat):
    hits = []
    cols = active_cols(pat)
    for y in range(H - len(pat) + 1):
        for x in range(W - 3):
            ok = True
            for r, byte in enumerate(pat):
                for c in cols:
                    want = bool(byte & (0x80 >> c))
                    got = fb[(y + r) * W + x + c] != 0
                    if want != got:
                        ok = False
                        break
                if not ok:
                    break
            if ok:
                hits.append((x, y))
    return hits


oks = find(OK)
nos = find(NO)

# Real test markers only live in the four result columns; the other hits
# are label glyphs that happen to match the pattern.
RESULT_X = {11, 27, 43, 59}
oks = [h for h in oks if h[0] in RESULT_X]
nos = [h for h in nos if h[0] in RESULT_X]

print(f"ok markers: {len(oks)} at {oks}")
print(f"no markers: {len(nos)} at {nos}")
sys.exit(0 if len(oks) == 22 and len(nos) == 0 else 1)
