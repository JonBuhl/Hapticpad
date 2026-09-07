#!/usr/bin/env python3
"""Erzeugt die 15x15 1-Bit-BMP Icons fuer das Taskbar-Profil.

Schreibt "Example SD Card/Taskbar/1.bmp" .. "6.bmp": je eine grosse Ziffer.
Die Ziffer ist die Taskleisten-Position, die Win+N ansteuert — sie bleibt also
richtig, egal welche App der Nutzer dort anpinnt. Welche App das ist, steht im
<Label> des Buttons.

Aufruf:  python3 scripts/generate_taskbar_icons.py
"""

import os
import sys

from PIL import Image, ImageDraw

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGET = os.path.join(REPO, "Example SD Card", "Taskbar")
SIZE = 15

# Sieben-Segment-Raster, mittig im 15x15 Feld.
LEFT, RIGHT = 4, 10
TOP, MID, BOTTOM = 2, 7, 12

SEGMENTS = {
    "a": ((LEFT, TOP), (RIGHT, TOP)),
    "b": ((RIGHT, TOP), (RIGHT, MID)),
    "c": ((RIGHT, MID), (RIGHT, BOTTOM)),
    "d": ((LEFT, BOTTOM), (RIGHT, BOTTOM)),
    "e": ((LEFT, MID), (LEFT, BOTTOM)),
    "f": ((LEFT, TOP), (LEFT, MID)),
    "g": ((LEFT, MID), (RIGHT, MID)),
}

DIGITS = {
    2: "abged",
    3: "abgcd",
    4: "fbgc",
    5: "afgcd",
    6: "afgecd",
}


def make_icon(draw_fn):
    img = Image.new("1", (SIZE, SIZE), 0)
    draw_fn(ImageDraw.Draw(img))
    return img


def one_icon(d):
    # Die 1 bekommt eine eigene Form: als Segment-Ziffer stuende sie schief
    # am rechten Rand statt in der Mitte.
    d.line([(7, TOP), (7, BOTTOM)], fill=1, width=1)
    d.line([(5, TOP + 2), (7, TOP)], fill=1, width=1)
    d.line([(5, BOTTOM), (9, BOTTOM)], fill=1, width=1)


def segment_icon(digit):
    def draw(d):
        for name in DIGITS[digit]:
            start, end = SEGMENTS[name]
            d.line([start, end], fill=1, width=1)

    return draw


def main():
    os.makedirs(TARGET, exist_ok=True)

    for index in range(1, 7):
        draw_fn = one_icon if index == 1 else segment_icon(index)
        path = os.path.join(TARGET, "%d.bmp" % index)
        make_icon(draw_fn).save(path, format="BMP")
        print("geschrieben: %s" % os.path.relpath(path, REPO))

    return 0


if __name__ == "__main__":
    sys.exit(main())
