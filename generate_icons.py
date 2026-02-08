from PIL import Image, ImageDraw, ImageFont
import os

BASE = r"F:\\"
SIZE = 15
FONT = ImageFont.load_default()


def centered_text(draw: ImageDraw.ImageDraw, text: str) -> None:
    bbox = draw.textbbox((0, 0), text, font=FONT)
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    draw.text(((SIZE - w) / 2, (SIZE - h) / 2), text, 1, font=FONT)


def make_icon(draw_fn):
    img = Image.new("1", (SIZE, SIZE), 0)
    draw = ImageDraw.Draw(img)
    draw_fn(draw)
    return img


def num_icon(n: int):
    return lambda d: centered_text(d, str(n))


def arrow_left(d):
    d.polygon([(4, 7), (11, 2), (11, 12)], fill=1)


def arrow_right(d):
    d.polygon([(11, 7), (4, 2), (4, 12)], fill=1)


def arrow_up(d):
    d.polygon([(7, 2), (2, 11), (12, 11)], fill=1)


def arrow_down(d):
    d.polygon([(7, 12), (2, 3), (12, 3)], fill=1)


def double_vertical(d):
    d.line([(7, 2), (7, 12)], fill=1, width=2)
    d.polygon([(7, 0), (4, 4), (10, 4)], fill=1)
    d.polygon([(7, 14), (4, 10), (10, 10)], fill=1)


def play_icon(d):
    d.polygon([(5, 2), (11, 7), (5, 12)], fill=1)


def star_icon(d):
    d.line([(2, 2), (12, 12)], fill=1, width=1)
    d.line([(12, 2), (2, 12)], fill=1, width=1)
    d.line([(7, 0), (7, 14)], fill=1, width=1)
    d.line([(0, 7), (14, 7)], fill=1, width=1)


def search_icon(d):
    d.ellipse([(3, 3), (10, 10)], outline=1, width=1)
    d.line([(9, 9), (13, 13)], fill=1, width=1)


def save_icon(d):
    d.rectangle([(2, 3), (12, 12)], outline=1, fill=0)
    d.rectangle([(3, 4), (11, 9)], fill=1)
    d.rectangle([(4, 10), (10, 11)], fill=0)
    d.line([(3, 6), (11, 6)], fill=0, width=1)


def extrude_icon(d):
    d.rectangle([(4, 5), (10, 12)], outline=1, fill=0)
    d.line([(7, 1), (7, 5)], fill=1, width=1)
    d.polygon([(7, 0), (4, 3), (10, 3)], fill=1)


def line_icon(d):
    d.line([(2, 12), (12, 2)], fill=1, width=1)


def refresh_icon(d):
    d.arc([(2, 2), (12, 12)], start=45, end=315, fill=1, width=1)
    d.polygon([(9, 2), (13, 2), (11, 6)], fill=1)


def plus_icon(d):
    d.line([(7, 2), (7, 12)], fill=1, width=2)
    d.line([(2, 7), (12, 7)], fill=1, width=2)


def x_icon(d):
    d.line([(3, 3), (11, 11)], fill=1, width=2)
    d.line([(11, 3), (3, 11)], fill=1, width=2)


def letter_icon(ch: str):
    return lambda d: centered_text(d, ch)


def speaker_shape(d):
    d.polygon([(2, 6), (5, 6), (9, 3), (9, 12), (5, 9), (2, 9)], fill=1)


def volume_up_icon(d):
    speaker_shape(d)
    d.line([(11, 5), (11, 10)], fill=1, width=1)
    d.line([(11, 7), (13, 7)], fill=1, width=1)
    d.line([(13, 5), (13, 10)], fill=1, width=1)


def volume_down_icon(d):
    speaker_shape(d)
    d.line([(11, 7), (13, 7)], fill=1, width=1)


def mute_icon(d):
    speaker_shape(d)
    d.line([(11, 4), (13, 10)], fill=1, width=2)


def lock_icon(d):
    d.rectangle([(4, 7), (11, 13)], outline=1, fill=0)
    d.rectangle([(5, 8), (10, 12)], outline=1, fill=0)
    d.arc([(4, 2), (11, 9)], start=200, end=340, fill=1, width=1)


def tiles_icon(d):
    d.rectangle([(2, 2), (7, 7)], outline=1, fill=0)
    d.rectangle([(8, 2), (13, 7)], outline=1, fill=0)
    d.rectangle([(2, 8), (7, 13)], outline=1, fill=0)
    d.rectangle([(8, 8), (13, 13)], outline=1, fill=0)


def clipboard_icon(d):
    d.rectangle([(3, 4), (12, 13)], outline=1, fill=0)
    d.rectangle([(5, 1), (10, 4)], outline=1, fill=0)
    d.line([(6, 3), (9, 3)], fill=1, width=1)


def press_pull_icon(d):
    d.line([(7, 3), (7, 12)], fill=1, width=1)
    d.polygon([(7, 2), (4, 5), (10, 5)], fill=1)
    d.polygon([(7, 13), (4, 10), (10, 10)], fill=1)


def save_profile_icons(profile, icons):
    folder = os.path.join(BASE, profile)
    os.makedirs(folder, exist_ok=True)
    for idx, icon_fn in enumerate(icons, start=1):
        img = make_icon(icon_fn)
        img.save(os.path.join(folder, f"{idx}.bmp"), format="BMP")


profiles = {
    "Clicky": [num_icon(i) for i in range(1, 7)],
    "Twist": [num_icon(i) for i in range(1, 7)],
    "Momentum": [num_icon(i) for i in range(1, 7)],
    "Blender": [
        save_icon,
        arrow_left,
        arrow_right,
        play_icon,
        star_icon,
        search_icon,
    ],
    "Fusion 360": [
        arrow_left,
        arrow_right,
        extrude_icon,
        press_pull_icon,
        line_icon,
        search_icon,
    ],
    "Web Browsing": [
        plus_icon,
        x_icon,
        refresh_icon,
        arrow_left,
        arrow_right,
        letter_icon("A"),
    ],
    "Computer Control": [
        volume_up_icon,
        volume_down_icon,
        mute_icon,
        lock_icon,
        tiles_icon,
        clipboard_icon,
    ],
}

if __name__ == "__main__":
    for profile, icon_set in profiles.items():
        save_profile_icons(profile, icon_set)
    print("Icons written to SD card.")

# python generate_icons.py