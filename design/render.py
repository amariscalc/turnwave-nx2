"""Render the native UI and its documentation preview from the same image."""

from pathlib import Path
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

WIDTH, HEIGHT = 1280, 720
FONTS = Path(os.environ.get("TURNWAVE_FONTS", "/usr/share/fonts/truetype/open-sans"))
OUT = Path(sys.argv[1])
OUT.mkdir(parents=True, exist_ok=True)
y, x = np.mgrid[:HEIGHT, :WIDTH].astype(np.float32)
base = np.broadcast_to(np.array([10, 15, 24], dtype=np.float32), (HEIGHT, WIDTH, 3)).copy()
for cx, cy, sx, sy, color in [
    (1010, 370, 415, 325, (42, 25, 78)),
    (1110, 90, 270, 220, (28, 52, 88)),
    (730, 660, 365, 175, (13, 43, 59)),
    (1180, 650, 225, 280, (35, 16, 66)),
    (220, 180, 280, 325, (10, 12, 32)),
]:
    glow = np.exp(-((x - cx) / sx) ** 2 - ((y - cy) / sy) ** 2)
    base += glow[..., None] * color

# The rings are sampled per pixel to give the glass fine highlights and depth.
dx = x - 937
dy = (y - 371) * 1.10
angle = np.arctan2(dy, dx)
radius = np.hypot(dx, dy) + 10 * np.cos(3 * angle + .5) + 6 * np.sin(2 * angle)
surface = np.exp(-((radius - 178) / 50) ** 4)
light = np.maximum(0, .55 + .45 * np.sin(2 * angle + .8))
cyan = .5 + .5 * np.cos(angle + 1.8)
rose = .5 + .5 * np.sin(angle + .25)
base += (surface * (.12 + .44 * light))[..., None] * np.stack(
    [80 + 110 * rose, 98 + 85 * cyan, np.full_like(x, 180)], axis=-1
)
for ring, thickness, power in [(133, 3.7, .85), (145, 2.4, .28), (170, 1.8, .15),
                               (196, 3.2, .36), (215, 3.3, .93), (224, 1.2, .33)]:
    reflection = np.exp(-((radius-ring) / thickness) ** 2) * power * (.18 + .82 * light)
    base += reflection[..., None] * np.stack(
        [110 + 100 * rose, 88 + 144 * cyan, np.full_like(x, 225)], axis=-1
    )
for ring in (244, 267, 291):
    orbit = np.exp(-((radius - ring) / .85) ** 2) * (.2 + .8 * light)
    base += orbit[..., None] * (23, 30, 49)

art = Image.fromarray(np.clip(base, 0, 255).astype("uint8")).convert("RGBA")


def glass(box, corner, tint, edge, shine=70):
    mask = Image.new("L", art.size)
    ImageDraw.Draw(mask).rounded_rectangle(box, corner, fill=255)
    blurred = art.filter(ImageFilter.GaussianBlur(17))
    art.paste(Image.blend(blurred, Image.new("RGBA", art.size, (*tint, 255)), .36), (0, 0), mask)
    highlight = np.zeros((HEIGHT, WIDTH, 4), dtype=np.uint8)
    highlight[:, :, :3] = (228, 235, 255)
    fade = np.clip(1 - (y - box[1]) / (box[3] - box[1]), 0, 1)
    highlight[:, :, 3] = (np.asarray(mask) / 255 * shine * fade**2).astype("uint8")
    art.alpha_composite(Image.fromarray(highlight))
    ImageDraw.Draw(art).rounded_rectangle(box, corner, outline=edge, width=1)


def label(pos, value, size, weight="Regular", color="#ffffff", anchor="lt"):
    ImageDraw.Draw(art).text(
        pos, value, font=ImageFont.truetype(str(FONTS / f"OpenSans-{weight}.ttf"), size),
        fill=color, anchor=anchor,
    )


glass((64, 48, 244, 96), 24, (29, 34, 60), "#5b5c82", 37)
draw = ImageDraw.Draw(art)
draw.ellipse((84, 63, 100, 79), outline="#dbd7ff", width=2)
draw.ellipse((89, 68, 95, 74), fill="#b6a7ff")
label((112, 71), "Turnwave NX2", 16, "Bold", anchor="lm")

glass((64, 150, 640, 581), 38, (18, 22, 40), "#68698f", 32)
label((104, 201), "Apple Music", 65, "Bold")
label((107, 291), "Explore the catalog and listen to your", 22, color="#d1d7ed")
label((107, 323), "library on the official web player.", 22, color="#d1d7ed")

glass((104, 424, 466, 495), 35, (106, 76, 196), "#bfb4ff", 47)
draw = ImageDraw.Draw(art)
draw.ellipse((121, 439, 161, 479), fill="#ddd7ff")
label((141, 459), "A", 20, "Bold", "#31225f", "mm")
label((180, 459), "Open Apple Music", 23, "Bold", anchor="lm")
draw.line((424, 459, 444, 459), fill="#ded6ff", width=2)
draw.line([(437, 452), (444, 459), (437, 466)], fill="#ded6ff", width=2)

glass((862, 296, 1012, 446), 43, (51, 45, 89), "#a89bd7", 71)
draw = ImageDraw.Draw(art)
draw.rounded_rectangle((873, 307, 1001, 435), 35, outline="#6e649c")
draw.line([(899, 373), (914, 373), (923, 352), (937, 395), (950, 347), (962, 373), (975, 373)],
          fill="#f0eaff", width=5, joint="curve")

glass((64, 632, 242, 676), 22, (28, 42, 64), "#555d78", 24)
draw = ImageDraw.Draw(art)
draw.ellipse((77, 643, 99, 665), outline="#aab5d0", width=1)
label((88, 654), "+", 17, "Bold", "#ebf0ff", "mm")
label((110, 655), "Exit to HBMenu", 15, "Semibold", "#d6def4", "lm")

# Keep the error and progress overlays inside the card's status line.
art.save(OUT / "launcher-preview.png", optimize=True)
(OUT / "home.rgba").write_bytes(art.tobytes())
art = art.copy()
label((108, 526), "Opening the Apple Music website…", 18, "Semibold", "#e0d9fb")
(OUT / "opening.rgba").write_bytes(art.crop((100, 515, 611, 576)).tobytes())
art = Image.open(OUT / "launcher-preview.png")
label((108, 526), "Couldn’t open Apple Music. Press A to retry.", 18, "Semibold", "#ffd0db")
label((108, 550), "Error 0x", 14, "Semibold", "#ffd0db")
(OUT / "failed.rgba").write_bytes(art.crop((100, 515, 611, 576)).tobytes())
art.save(OUT / "error-preview.png", optimize=True)
glyphs = bytearray()
for value in "0123456789ABCDEF":
    mask = Image.new("L", (12, 18))
    ImageDraw.Draw(mask).text((0, 0), value,
        font=ImageFont.truetype(str(FONTS / "OpenSans-Semibold.ttf"), 14), fill=255, anchor="lt")
    glyphs.extend(mask.tobytes())
(OUT / "hex.alpha").write_bytes(glyphs)
