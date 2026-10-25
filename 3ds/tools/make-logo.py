#!/usr/bin/env python3
"""Draws the logo of the 3DS build: "Achtung, die Kurve!" in the pixel font. Writes
the title logo (assets/gfx/logo.png), the banner of the CIA (banner.png) and the
icon (icon.png): a road sign that warns of a curve. Needs Pillow.

    python3 3ds/tools/make-logo.py
"""
import os

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = os.path.join(HERE, "..", "assets", "fonts", "PressStart2P-Regular.ttf")

NAVY = (18, 22, 46)
RED, YELLOW = (255, 107, 107), (255, 200, 87)


def text_mask(text, k):
    """The text in the font's own 8x8 grid, every pixel k pixels big."""
    font = ImageFont.truetype(FONT, 8)
    mask = Image.new("L", (8 * len(text), 8), 0)
    ImageDraw.Draw(mask).text((0, 0), text, font=font, fill=255)
    mask = mask.point(lambda v: 255 if v > 127 else 0)
    return mask.resize((mask.width * k, mask.height * k), Image.NEAREST)


def gradient(size, top, bottom):
    img = Image.new("RGB", size)
    px = img.load()
    for y in range(size[1]):
        t = y / max(1, size[1] - 1)
        c = tuple(round(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        for x in range(size[0]):
            px[x, y] = c
    return img


def put_text(canvas, text, k, x, y, top, bottom, shadow):
    """Text with an outline, a hard shadow and a vertical gradient."""
    mask = text_mask(text, k)
    outline = mask.filter(ImageFilter.MaxFilter(2 * k + 1))
    pad = 2 * k
    for dx, dy, color, m in ((k, k, shadow, outline), (0, 0, NAVY, outline)):
        layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
        layer.paste(color + (255,), (x - k + dx, y - k + dy), m)
        canvas.alpha_composite(layer)
    fill = gradient(mask.size, top, bottom)
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    layer.paste(fill, (x, y), mask)
    canvas.alpha_composite(layer)
    return mask.size



def logo(width, k1, k2, height):
    canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    w1 = 8 * 8 * k1
    w2 = 10 * 8 * k2
    gap = 3 * k1
    top = (height - (8 * k1 + gap + 8 * k2)) // 2
    put_text(canvas, "ACHTUNG,", k1, (width - w1) // 2, top, (255, 232, 160),
             YELLOW, (150, 60, 20))
    put_text(canvas, "DIE KURVE!", k2, (width - w2) // 2, top + 8 * k1 + gap,
             (255, 255, 255), (170, 195, 255), (170, 30, 50))
    return canvas


def sign():
    """The icon: the road sign "curve ahead", a triangle with a bent arrow."""
    icon = gradient((48, 48), (27, 33, 64), (10, 12, 30)).convert("RGBA")
    d = ImageDraw.Draw(icon)
    d.polygon([(24, 1), (47, 44), (1, 44)], fill=(214, 40, 40, 255))
    d.polygon([(24, 10), (40, 39), (8, 39)], fill=(255, 255, 255, 255))
    dark = (24, 24, 32, 255)
    # The way: up from the bottom, then a bend to the right.
    path = [(19, 37), (19, 31), (20, 27), (22, 25), (25, 24)]
    d.line(path, fill=dark, width=3)
    d.polygon([(24, 19), (31, 24), (24, 29)], fill=dark)
    return icon


def main():
    out = os.path.join(HERE, "..")
    gfx = os.path.join(out, "assets", "gfx")
    os.makedirs(gfx, exist_ok=True)

    # The title screen: over the animation of the trails.
    logo(360, 3, 4, 90).save(os.path.join(gfx, "logo.png"))

    # The banner of the CIA: 256x128, on a dark ground.
    banner = gradient((256, 128), (27, 33, 64), (10, 12, 30)).convert("RGBA")
    banner.alpha_composite(logo(256, 2, 3, 128))
    banner.convert("RGB").save(os.path.join(out, "banner.png"))

    sign().save(os.path.join(out, "icon.png"))


if __name__ == "__main__":
    main()
