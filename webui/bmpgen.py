#!/usr/bin/env python3
"""BMP-Erzeugung fuer die HapticPad-Icons.

Die Firmware (loadBMP15x15 in xmlData.ino) akzeptiert genau ein Format:
1 Bit Farbtiefe, 15x15 (bzw. 16x16 fuer loadBMP16x16), Zeilen auf 4 Byte
gepaddet. Pillow schreibt mode "1" exakt so: Palette 0=schwarz/1=weiss,
bottom-up, MSB zuerst — ein gesetztes Bit ist damit ein leuchtendes Pixel.

Alles laeuft ueber eine gemeinsame Pixel-Darstellung: eine Liste aus Strings
aus "0"/"1", eine Zeile pro String, oben zuerst. Die WebUI schickt sie hin und
her, dadurch ist der Pixel-Editor derselbe Weg wie jede Quelle.
"""

import io
import os

from PIL import Image, ImageChops, ImageDraw, ImageFont, ImageOps

SIZES = (15, 16)
DEFAULT_SIZE = 15


# --- Pixel-Darstellung ------------------------------------------------------

def image_to_pixels(img):
    img = img.convert("1")
    width, height = img.size
    data = img.load()
    return ["".join("1" if data[x, y] else "0" for x in range(width)) for y in range(height)]


def pixels_to_image(pixels):
    height = len(pixels)
    width = len(pixels[0]) if height else 0
    img = Image.new("1", (width, height), 0)
    data = img.load()
    for y, row in enumerate(pixels):
        for x, value in enumerate(row):
            if value == "1":
                data[x, y] = 1
    return img


def normalise_pixels(pixels, size):
    """Fremde/kaputte Pixelmatrix auf ein sauberes size x size Raster bringen."""
    rows = []
    for y in range(size):
        row = pixels[y] if y < len(pixels) else ""
        row = "".join("1" if c in ("1", 1, True) else "0" for c in str(row))
        rows.append((row + "0" * size)[:size])
    return rows


def pixels_to_bmp(pixels):
    buffer = io.BytesIO()
    pixels_to_image(pixels).save(buffer, format="BMP")
    return buffer.getvalue()


def bmp_to_pixels(data, size=DEFAULT_SIZE):
    img = Image.open(io.BytesIO(data)).convert("1")
    if img.size != (size, size):
        img = img.resize((size, size), Image.NEAREST)
    return image_to_pixels(img)


# --- Schriften --------------------------------------------------------------

FONT_DIRS = [
    os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts"),
    os.path.join(os.environ.get("LOCALAPPDATA", ""), "Microsoft", "Windows", "Fonts"),
    "/usr/share/fonts",
    "/usr/local/share/fonts",
    os.path.expanduser("~/.fonts"),
    os.path.expanduser("~/Library/Fonts"),
    "/Library/Fonts",
    "/System/Library/Fonts",
]

# Kleine Pixel-taugliche Schriften zuerst — die grossen Textsatz-Fonts sehen
# auf 15x15 fast alle gleich matschig aus.
FONT_PREFERENCE = [
    "smallest_pixel-7", "pixelmix", "silkscreen", "tahoma", "verdana",
    "arialbd", "arial", "segoeui", "dejavusans", "liberationsans",
]

_font_cache = None


def list_fonts():
    """Alle .ttf/.otf aus den ueblichen Systemordnern, kurze Namen zuerst."""
    global _font_cache
    if _font_cache is not None:
        return _font_cache

    found = {}
    for directory in FONT_DIRS:
        if not directory or not os.path.isdir(directory):
            continue
        try:
            entries = os.listdir(directory)
        except OSError:
            continue
        for entry in entries:
            if not entry.lower().endswith((".ttf", ".otf")):
                continue
            stem = os.path.splitext(entry)[0]
            found.setdefault(stem.lower(), dict(name=stem, path=os.path.join(directory, entry)))

    def sort_key(item):
        key = item["name"].lower()
        for index, preferred in enumerate(FONT_PREFERENCE):
            if key.startswith(preferred):
                return (0, index, key)
        return (1, 0, key)

    _font_cache = sorted(found.values(), key=sort_key)
    return _font_cache


def _load_font(path, size):
    if not path:
        return ImageFont.load_default()
    try:
        return ImageFont.truetype(path, size)
    except (OSError, ValueError):
        return ImageFont.load_default()


def _text_bbox(draw, text, font):
    try:
        return draw.textbbox((0, 0), text, font=font)
    except TypeError:  # sehr alte Pillow-Versionen
        width, height = draw.textsize(text, font=font)
        return (0, 0, width, height)


def _fit_font_size(text, path, box, max_size=64):
    """Groesste Schriftgroesse, bei der der Text noch in box x box passt."""
    best = 6
    probe = Image.new("1", (box * 4, box * 4), 0)
    draw = ImageDraw.Draw(probe)
    for size in range(6, max_size):
        font = _load_font(path, size)
        left, top, right, bottom = _text_bbox(draw, text, font)
        if (right - left) > box or (bottom - top) > box:
            break
        best = size
    return best


# --- Quellen ----------------------------------------------------------------

def render_text(text, size=DEFAULT_SIZE, font_path=None, font_size=0, bold=False,
                offset_x=0, offset_y=0, threshold=110):
    """Text oder ein Icon-Glyph mittig in ein size x size Feld setzen.

    Zwei Wege, weil sie unterschiedliche Sachen gut koennen:

    font_size > 0  rendert direkt in der angegebenen Pixelgroesse. Das ist das
                   Richtige fuer Pixel-Schriften und wenn man das Raster genau
                   treffen will.
    font_size = 0  rendert gross und rechnet herunter. Duenne Striche — bei
                   MDI-Glyphen die Regel — ueberleben nur so; direkt auf 1 Bit
                   gezeichnet fallen sie schlicht weg.
    """
    text = text or ""
    if not text:
        return ["0" * size for _ in range(size)]

    supersample = not font_size
    render_size = int(font_size) if font_size else max(size * 6, 72)
    font = _load_font(font_path, render_size)

    # Grosszuegige Flaeche und danach beschneiden: so haengt das Zentrieren
    # nicht an den Font-Metriken (Unterlaengen, Zeilenabstand).
    pad = render_size * 3
    canvas = Image.new("L", (pad, pad), 0)
    draw = ImageDraw.Draw(canvas)
    left, top, right, bottom = _text_bbox(draw, text, font)
    draw.text((pad // 3 - left, pad // 3 - top), text, fill=255, font=font)

    if bold:
        # Fett = ein Strich breiter. Auf 15x15 reicht das, echte Bold-Schnitte
        # fallen bei der Groesse ohnehin zu.
        width = max(1, render_size // 24) if supersample else 1
        shifted = Image.new("L", (pad, pad), 0)
        shifted.paste(canvas, (width, 0))
        canvas = ImageChops.lighter(canvas, shifted)

    box = canvas.getbbox()
    if box:
        canvas = canvas.crop(box)

    if supersample:
        canvas.thumbnail((size, size), Image.LANCZOS)
        mono = canvas.point(lambda v: 255 if v >= threshold else 0).convert("1")
    else:
        mono = canvas.point(lambda v: 255 if v >= 128 else 0).convert("1")
        clipped = mono.crop(mono.getbbox() or (0, 0, 1, 1))
        mono = clipped

    out = Image.new("1", (size, size), 0)
    x = (size - mono.size[0]) // 2 + int(offset_x)
    y = (size - mono.size[1]) // 2 + int(offset_y)
    out.paste(mono, (x, y))
    return image_to_pixels(out)


def render_image(data, size=DEFAULT_SIZE, threshold=128, invert=False, dither=False,
                 fit="contain", offset_x=0, offset_y=0, trim=True, gamma=1.0):
    """Beliebiges Bild auf 1 Bit herunterrechnen."""
    img = Image.open(io.BytesIO(data))

    # Transparenz auf schwarzem Grund flach legen, sonst wird sie weiss.
    if img.mode in ("RGBA", "LA", "PA") or "transparency" in img.info:
        img = img.convert("RGBA")
        flat = Image.new("RGBA", img.size, (0, 0, 0, 255))
        flat.alpha_composite(img)
        img = flat
    img = img.convert("L")

    if trim:
        # Rand wegschneiden, damit kleine Motive das Feld ausfuellen.
        mask = img.point(lambda v: 255 if v >= threshold else 0)
        box = mask.getbbox()
        if box and (box[2] - box[0]) > 0 and (box[3] - box[1]) > 0:
            img = img.crop(box)

    if gamma and gamma != 1.0:
        img = img.point(lambda v: int(255 * ((v / 255.0) ** (1.0 / max(gamma, 0.05)))))

    if fit == "stretch":
        img = img.resize((size, size), Image.LANCZOS)
        canvas = img
    else:
        target = img.copy()
        if fit == "cover":
            target = ImageOps.fit(target, (size, size), Image.LANCZOS)
        else:
            target.thumbnail((size, size), Image.LANCZOS)
        canvas = Image.new("L", (size, size), 0)
        canvas.paste(target, ((size - target.size[0]) // 2, (size - target.size[1]) // 2))

    if dither:
        mono = canvas.convert("1")
    else:
        mono = canvas.point(lambda v: 255 if v >= threshold else 0).convert("1")

    if invert:
        mono = ImageOps.invert(mono.convert("L")).convert("1")

    if offset_x or offset_y:
        shifted = Image.new("1", (size, size), 0)
        shifted.paste(mono, (int(offset_x), int(offset_y)))
        mono = shifted

    return image_to_pixels(mono)


def render_glyph(font_path, codepoint, size=DEFAULT_SIZE, font_size=0, offset_x=0,
                 offset_y=0, threshold=110):
    """Ein einzelnes Zeichen aus einer Icon-Schrift (z. B. MDI-Webfont)."""
    char = chr(int(codepoint)) if not isinstance(codepoint, str) else codepoint
    return render_text(char, size=size, font_path=font_path, font_size=font_size,
                       offset_x=offset_x, offset_y=offset_y, threshold=threshold)


# --- Eingebaute Formen ------------------------------------------------------
# Wachsen mit der Feldgroesse mit: Koordinaten sind auf 15x15 gedacht und
# werden fuer 16x16 um ein Pixel verschoben.

def _shapes(size):
    end = size - 1
    mid = size // 2

    def make(fn):
        def build():
            img = Image.new("1", (size, size), 0)
            fn(ImageDraw.Draw(img), size, end, mid)
            return image_to_pixels(img)
        return build

    def arrow_left(d, s, e, m):
        d.polygon([(3, m), (e - 3, 2), (e - 3, e - 2)], fill=1)

    def arrow_right(d, s, e, m):
        d.polygon([(e - 3, m), (3, 2), (3, e - 2)], fill=1)

    def arrow_up(d, s, e, m):
        d.polygon([(m, 2), (2, e - 3), (e - 2, e - 3)], fill=1)

    def arrow_down(d, s, e, m):
        d.polygon([(m, e - 2), (2, 3), (e - 2, 3)], fill=1)

    def play(d, s, e, m):
        d.polygon([(4, 2), (e - 3, m), (4, e - 2)], fill=1)

    def pause(d, s, e, m):
        d.rectangle([(4, 2), (6, e - 2)], fill=1)
        d.rectangle([(e - 6, 2), (e - 4, e - 2)], fill=1)

    def stop(d, s, e, m):
        d.rectangle([(3, 3), (e - 3, e - 3)], fill=1)

    def record(d, s, e, m):
        d.ellipse([(3, 3), (e - 3, e - 3)], fill=1)

    def plus(d, s, e, m):
        d.line([(m, 2), (m, e - 2)], fill=1, width=2)
        d.line([(2, m), (e - 2, m)], fill=1, width=2)

    def minus(d, s, e, m):
        d.line([(2, m), (e - 2, m)], fill=1, width=2)

    def cross(d, s, e, m):
        d.line([(3, 3), (e - 3, e - 3)], fill=1, width=2)
        d.line([(e - 3, 3), (3, e - 3)], fill=1, width=2)

    def check(d, s, e, m):
        d.line([(3, m), (m - 1, e - 4)], fill=1, width=2)
        d.line([(m - 1, e - 4), (e - 2, 3)], fill=1, width=2)

    def circle(d, s, e, m):
        d.ellipse([(2, 2), (e - 2, e - 2)], outline=1, width=1)

    def square(d, s, e, m):
        d.rectangle([(2, 2), (e - 2, e - 2)], outline=1, width=1)

    def search(d, s, e, m):
        d.ellipse([(2, 2), (e - 4, e - 4)], outline=1, width=1)
        d.line([(e - 5, e - 5), (e - 1, e - 1)], fill=1, width=2)

    def save(d, s, e, m):
        d.rectangle([(2, 2), (e - 2, e - 2)], outline=1, width=1)
        d.rectangle([(4, 3), (e - 4, m - 1)], fill=1)
        d.rectangle([(4, m + 2), (e - 4, e - 3)], outline=1, width=1)

    def folder(d, s, e, m):
        d.rectangle([(2, 4), (e - 2, e - 3)], outline=1, width=1)
        d.line([(2, 4), (5, 2)], fill=1, width=1)
        d.line([(5, 2), (m, 4)], fill=1, width=1)

    def refresh(d, s, e, m):
        d.arc([(2, 2), (e - 2, e - 2)], start=40, end=320, fill=1, width=1)
        d.polygon([(e - 5, 1), (e - 1, 3), (e - 5, 5)], fill=1)

    def undo(d, s, e, m):
        d.arc([(2, 3), (e - 2, e - 1)], start=180, end=350, fill=1, width=1)
        d.polygon([(1, 3), (5, 3), (3, 7)], fill=1)

    def redo(d, s, e, m):
        d.arc([(2, 3), (e - 2, e - 1)], start=190, end=360, fill=1, width=1)
        d.polygon([(e - 1, 3), (e - 5, 3), (e - 3, 7)], fill=1)

    def speaker(d, s, e, m):
        d.polygon([(2, m - 2), (5, m - 2), (8, 2), (8, e - 2), (5, m + 2), (2, m + 2)], fill=1)

    def volume_up(d, s, e, m):
        speaker(d, s, e, m)
        d.arc([(6, 3), (e - 1, e - 3)], start=300, end=60, fill=1, width=1)

    def volume_down(d, s, e, m):
        speaker(d, s, e, m)
        d.line([(10, m), (e - 2, m)], fill=1, width=1)

    def mute(d, s, e, m):
        speaker(d, s, e, m)
        d.line([(10, m - 3), (e - 2, m + 3)], fill=1, width=1)
        d.line([(e - 2, m - 3), (10, m + 3)], fill=1, width=1)

    def lock(d, s, e, m):
        d.rectangle([(3, m), (e - 3, e - 2)], outline=1, width=1)
        d.arc([(4, m - 5), (e - 4, m + 2)], start=180, end=360, fill=1, width=1)

    def grid(d, s, e, m):
        d.rectangle([(2, 2), (m - 1, m - 1)], outline=1, width=1)
        d.rectangle([(m + 1, 2), (e - 2, m - 1)], outline=1, width=1)
        d.rectangle([(2, m + 1), (m - 1, e - 2)], outline=1, width=1)
        d.rectangle([(m + 1, m + 1), (e - 2, e - 2)], outline=1, width=1)

    def sun(d, s, e, m):
        d.ellipse([(m - 3, m - 3), (m + 3, m + 3)], outline=1, width=1)
        for dx, dy in ((0, -6), (0, 6), (-6, 0), (6, 0), (-4, -4), (4, 4), (-4, 4), (4, -4)):
            d.point((m + dx, m + dy), fill=1)

    def brightness_up(d, s, e, m):
        sun(d, s, e, m)
        d.line([(e - 3, 1), (e - 3, 4)], fill=1, width=1)
        d.line([(e - 4, 2), (e - 1, 2)], fill=1, width=1)

    def brightness_down(d, s, e, m):
        sun(d, s, e, m)
        d.line([(e - 4, 2), (e - 1, 2)], fill=1, width=1)

    def home(d, s, e, m):
        d.polygon([(m, 2), (e - 2, m), (2, m)], fill=1)
        d.rectangle([(4, m), (e - 4, e - 2)], outline=1, width=1)

    def gear(d, s, e, m):
        d.ellipse([(4, 4), (e - 4, e - 4)], outline=1, width=1)
        for dx, dy in ((0, -6), (0, 6), (-6, 0), (6, 0)):
            d.line([(m, m), (m + dx, m + dy)], fill=1, width=1)

    def terminal(d, s, e, m):
        d.rectangle([(1, 2), (e - 1, e - 2)], outline=1, width=1)
        d.line([(3, 5), (6, m)], fill=1, width=1)
        d.line([(6, m), (3, e - 5)], fill=1, width=1)
        d.line([(8, e - 5), (e - 3, e - 5)], fill=1, width=1)

    def clipboard(d, s, e, m):
        d.rectangle([(3, 3), (e - 3, e - 2)], outline=1, width=1)
        d.rectangle([(m - 3, 1), (m + 3, 4)], outline=1, width=1)

    def window(d, s, e, m):
        d.rectangle([(1, 2), (e - 1, e - 2)], outline=1, width=1)
        d.line([(1, 5), (e - 1, 5)], fill=1, width=1)

    return {
        "arrow_left": make(arrow_left), "arrow_right": make(arrow_right),
        "arrow_up": make(arrow_up), "arrow_down": make(arrow_down),
        "play": make(play), "pause": make(pause), "stop": make(stop), "record": make(record),
        "plus": make(plus), "minus": make(minus), "cross": make(cross), "check": make(check),
        "circle": make(circle), "square": make(square), "search": make(search),
        "save": make(save), "folder": make(folder), "refresh": make(refresh),
        "undo": make(undo), "redo": make(redo),
        "volume_up": make(volume_up), "volume_down": make(volume_down), "mute": make(mute),
        "lock": make(lock), "grid": make(grid), "sun": make(sun),
        "brightness_up": make(brightness_up), "brightness_down": make(brightness_down),
        "home": make(home), "gear": make(gear), "terminal": make(terminal),
        "clipboard": make(clipboard), "window": make(window),
    }


SHAPE_LABELS = {
    "arrow_left": "Pfeil links", "arrow_right": "Pfeil rechts",
    "arrow_up": "Pfeil hoch", "arrow_down": "Pfeil runter",
    "play": "Play", "pause": "Pause", "stop": "Stop", "record": "Aufnahme",
    "plus": "Plus", "minus": "Minus", "cross": "Kreuz / Schliessen", "check": "Haken",
    "circle": "Kreis", "square": "Quadrat", "search": "Lupe",
    "save": "Diskette", "folder": "Ordner", "refresh": "Neu laden",
    "undo": "Rueckgaengig", "redo": "Wiederholen",
    "volume_up": "Lauter", "volume_down": "Leiser", "mute": "Stumm",
    "lock": "Schloss", "grid": "Kacheln", "sun": "Sonne",
    "brightness_up": "Heller", "brightness_down": "Dunkler",
    "home": "Haus", "gear": "Zahnrad", "terminal": "Terminal",
    "clipboard": "Zwischenablage", "window": "Fenster",
}


def list_shapes(size=DEFAULT_SIZE):
    """Alle Formen samt fertiger Vorschau — die UI braucht beides auf einmal."""
    shapes = _shapes(size)
    return [dict(id=key, label=SHAPE_LABELS.get(key, key), pixels=shapes[key]())
            for key in sorted(SHAPE_LABELS)]


def render_shape(name, size=DEFAULT_SIZE):
    shapes = _shapes(size)
    if name not in shapes:
        raise KeyError("Unbekannte Form: %s" % name)
    return shapes[name]()


# --- Nachbearbeitung --------------------------------------------------------

def transform(pixels, invert=False, flip_h=False, flip_v=False, rotate=0,
              shift_x=0, shift_y=0):
    size = len(pixels)
    img = pixels_to_image(pixels)

    if rotate:
        img = img.rotate(-int(rotate) % 360, expand=False, fillcolor=0)
    if flip_h:
        img = ImageOps.mirror(img)
    if flip_v:
        img = ImageOps.flip(img)
    if shift_x or shift_y:
        moved = Image.new("1", img.size, 0)
        moved.paste(img, (int(shift_x), int(shift_y)))
        img = moved
    if invert:
        img = ImageOps.invert(img.convert("L")).convert("1")

    return normalise_pixels(image_to_pixels(img), size)
