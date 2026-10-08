"""5-quirks verification: six flag markers in the right column."""
import sys

W = 64
OK = [0xA0, 0xC0, 0x80]
ERR = [0xA0, 0x40, 0xA0]
ROWS = {2: "vfQuirk", 7: "memQuirk", 12: "display-wait",
        17: "clipping", 22: "shift", 27: "jump"}

fb = open(sys.argv[1], "rb").read()
assert len(fb) == W * 32


def glyph(pat, x, y):
    return all(bool(fb[(y + r) * W + x + c]) == bool(b & (0x80 >> c))
               for r, b in enumerate(pat) for c in range(3))


passed = 0
for y, name in ROWS.items():
    if glyph(OK, 59, y):
        print(f"OK   {name}")
        passed += 1
    elif glyph(ERR, 59, y):
        print(f"FAIL {name}")
    else:
        print(f"MISS {name}")
print(f"quirks: {passed}/6")
sys.exit(0 if passed == 6 else 1)
