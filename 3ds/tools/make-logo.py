#!/usr/bin/env python3
"""Draws the logo of the 3DS build: "Achtung, die Kurve!" in the pixel font, over
a ribbon of four trails. Writes the title logo (assets/gfx/logo.png) and the
banner of the CIA (banner.png). Needs Pillow.

    python3 3ds/tools/make-logo.py
"""
import math
import os

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = os.path.join(HERE, "..", "assets", "fonts", "PressStart2P-Regular.ttf")

NAVY = (18, 22, 46)
RED, BLUE, GREEN, YELLOW = (255, 107, 107), (95, 174, 255), (95, 215, 120), (255, 200, 87)
TRAILS = [RED, BLUE, GREEN, YELLOW]


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


def ribbon(canvas, y0, amp, unit, width):
    """Four trails side by side, with holes, each ending in a head."""
    d = ImageDraw.Draw(canvas)
    gap = unit * 4
    holes = [(0.30, 0.34), (0.55, 0.60), (0.20, 0.23), (0.72, 0.77)]
    ends = [0.86, 0.92, 0.98, 0.95]
    for i, color in enumerate(TRAILS):
        x = 0
        end = int(width * ends[i])
        last = None
        while x < end:
            t = x / width
            if not any(a <= t <= b for a, b in holes[i:i + 1] + holes[(i + 2) % 4:(i + 2) % 4 + 1]):
                y = y0 + (i - 1.5) * gap + amp * math.sin(t * 2 * math.pi * 1.5 + 0.6)
                gx, gy = round(x / unit) * unit, round(y / unit) * unit
                if (gx, gy) != last:
                    d.rectangle([gx, gy, gx + unit * 2 - 1, gy + unit * 2 - 1],
                                fill=color + (255,))
                    last = (gx, gy)
            x += 1
        # the head: a bigger, lighter diamond where the trail stops
        t = end / width
        y = y0 + (i - 1.5) * gap + amp * math.sin(t * 2 * math.pi * 1.5 + 0.6)
        cx, cy = round(end / unit) * unit, round(y / unit) * unit
        light = tuple((c + 255) // 2 for c in color)
        for ox, oy in ((0, 0), (-1, 0), (1, 0), (0, -1), (0, 1)):
            d.rectangle([cx + ox * unit, cy + oy * unit,
                         cx + (ox + 1) * unit, cy + (oy + 1) * unit],
                        fill=light + (255,))


def logo(width, k1, k2, unit, amp, height):
    canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    w1 = 8 * 8 * k1
    w2 = 10 * 8 * k2
    y = 4 * k1
    put_text(canvas, "ACHTUNG,", k1, (width - w1) // 2, y, (255, 232, 160),
             YELLOW, (150, 60, 20))
    y += 8 * k1 + 3 * k1
    put_text(canvas, "DIE KURVE!", k2, (width - w2) // 2, y, (255, 255, 255),
             (170, 195, 255), (170, 30, 50))
    y += 8 * k2 + 4 * unit
    ribbon(canvas, y + (height - y) // 2, amp, unit, width)
    return canvas


def main():
    out = os.path.join(HERE, "..")
    gfx = os.path.join(out, "assets", "gfx")
    os.makedirs(gfx, exist_ok=True)

    # The title screen: 360 pixels wide, over the animation of the trails.
    logo(360, 3, 4, 3, 19, 170).save(os.path.join(gfx, "logo.png"))

    # The banner of the CIA: 256x128, on a dark ground.
    banner = Image.new("RGBA", (256, 128), NAVY + (255,))
    ground = gradient((256, 128), (27, 33, 64), (10, 12, 30)).convert("RGBA")
    banner.alpha_composite(ground)
    small = logo(240, 2, 3, 2, 10, 120)
    banner.alpha_composite(small, (8, 4))
    banner.convert("RGB").save(os.path.join(out, "banner.png"))

    # The icon in the HOME menu: 48x48, the ribbon bending over the dark ground.
    icon = gradient((48, 48), (27, 33, 64), (10, 12, 30)).convert("RGBA")
    layer = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    ribbon(layer, 24, 9, 2, 48)
    icon.alpha_composite(layer)
    icon.save(os.path.join(out, "icon.png"))


if __name__ == "__main__":
    main()
