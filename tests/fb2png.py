"""Render a dump.c framebuffer file (64x32, one byte per pixel) to PNG.

Usage: fb2png.py <framebuffer.bin> <out.png> [scale]

The colors match the SDL renderer: white pixels on black.
"""
import sys

from PIL import Image

W, H = 64, 32

data = open(sys.argv[1], "rb").read()
assert len(data) == W * H, "expected 64x32 framebuffer"

scale = int(sys.argv[3]) if len(sys.argv) > 3 else 10

img = Image.new("L", (W, H))
img.putdata([255 if p else 0 for p in data])
img = img.resize((W * scale, H * scale), Image.NEAREST)
img.convert("RGB").save(sys.argv[2])
print(sys.argv[2])
