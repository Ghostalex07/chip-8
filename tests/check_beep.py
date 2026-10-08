"""7-beep verification: the sound icon appears and disappears again.

The ROM draws a beeping icon shortly after start and erases it once the
sound timer has run out, so the framebuffer at two points in time shows
whether the timing works.
"""
import sys

on = sum(open(sys.argv[1], "rb").read())
off = sum(open(sys.argv[2], "rb").read())
print(f"icon pixels: on={on} off={off}")
sys.exit(0 if on > 0 and off == 0 else 1)
