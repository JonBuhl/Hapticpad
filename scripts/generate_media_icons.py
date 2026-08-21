#!/usr/bin/env python3
"""Erzeugt die 15x15 1-Bit-BMP Icons fuer das Media-Profil.

Schreibt "Example SD Card/Media/1.bmp" .. "6.bmp" passend zu den sechs Buttons
des Media-Profils in config.yaml.

Aufruf:  python3 scripts/generate_media_icons.py
"""

import os
import sys

from PIL import Image, ImageDraw

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGET = os.path.join(REPO, "Example SD Card", "Media")
SIZE = 15


def make_icon(draw_fn):
    img = Image.new("1", (SIZE, SIZE), 0)
    draw_fn(ImageDraw.Draw(img))
    return img


def speaker_shape(d):
    d.polygon([(2, 6), (5, 6), (9, 3), (9, 12), (5, 9), (2, 9)], fill=1)


def volume_icon(d):
    # Lautsprecher mit zwei Schallwellen.
    speaker_shape(d)
    d.line([(11, 5), (11, 10)], fill=1, width=1)
    d.line([(13, 3), (13, 12)], fill=1, width=1)


def mute_icon(d):
    speaker_shape(d)
    d.line([(11, 4), (14, 11)], fill=1, width=1)
    d.line([(14, 4), (11, 11)], fill=1, width=1)


def wave_icon(d):
    # Wellenform: Balken unterschiedlicher Hoehe, steht fuer Scratch/Spulen.
    for x, half in ((1, 2), (3, 6), (5, 3), (7, 5), (9, 1), (11, 4), (13, 2)):
        d.line([(x, 7 - half), (x, 7 + half)], fill=1, width=1)


def play_pause_icon(d):
    d.polygon([(1, 2), (7, 7), (1, 12)], fill=1)
    d.rectangle([(9, 2), (10, 12)], fill=1)
    d.rectangle([(12, 2), (13, 12)], fill=1)


def next_icon(d):
    d.polygon([(2, 3), (6, 7), (2, 11)], fill=1)
    d.polygon([(7, 3), (11, 7), (7, 11)], fill=1)
    d.rectangle([(12, 3), (13, 11)], fill=1)


def prev_icon(d):
    d.rectangle([(1, 3), (2, 11)], fill=1)
    d.polygon([(7, 3), (3, 7), (7, 11)], fill=1)
    d.polygon([(12, 3), (8, 7), (12, 11)], fill=1)


# Reihenfolge = Buttons 1..6 des Media-Profils.
ICONS = [
    volume_icon,      # 1 Volume (Wheel-Domain)
    wave_icon,        # 2 Scratch (Wheel-Domain)
    play_pause_icon,  # 3 Play/Pause
    next_icon,        # 4 Next
    prev_icon,        # 5 Prev
    mute_icon,        # 6 Mute
]


def main():
    os.makedirs(TARGET, exist_ok=True)

    for index, icon_fn in enumerate(ICONS, start=1):
        path = os.path.join(TARGET, "%d.bmp" % index)
        make_icon(icon_fn).save(path, format="BMP")
        print("geschrieben: %s" % os.path.relpath(path, REPO))

    return 0


if __name__ == "__main__":
    sys.exit(main())
