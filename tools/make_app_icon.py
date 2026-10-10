#!/usr/bin/env python3
"""
Draw the app icon (icons/app.png, icons/app.ico): a white egg with three
bright green spots, the project's chosen design, drawn from simple shapes.

Usage: python3 tools/make_app_icon.py   (needs Pillow)
To use a hand-made PNG instead, save it as icons/app.png and run with
--from-png to rebuild app.ico from it.
The Windows resource icons/app.res is then built with:
    echo '1 ICON "app.ico"' > app.rc && llvm-rc /FO app.res app.rc
"""
import math
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

S = 2048  # drawn large, scaled down to 512 for smooth edges
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "icons")
ICO_SIZES = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]


def egg_polygon(cx, cy, rx, ry, tilt_deg):
    """Egg outline: narrower at the top, tilted slightly to the right."""
    pts = []
    tilt = math.radians(tilt_deg)
    for i in range(720):
        a = 2 * math.pi * i / 720
        x = math.cos(a)
        y = math.sin(a)  # -1 top, +1 bottom
        x *= 0.88 + 0.12 * y  # egg taper
        px, py = x * rx, y * ry
        qx = px * math.cos(tilt) - py * math.sin(tilt)
        qy = px * math.sin(tilt) + py * math.cos(tilt)
        pts.append((cx + qx, cy + qy))
    return pts


def blob(draw, cx, cy, rx, ry, rot_deg, squareness, fill):
    """A rounded blob between an ellipse (squareness 0) and a squircle."""
    pts = []
    r = math.radians(rot_deg)
    n = 2.0 + squareness * 2.0
    for i in range(360):
        a = 2 * math.pi * i / 360
        c, s = math.cos(a), math.sin(a)
        x = rx * math.copysign(abs(c) ** (2 / n), c)
        y = ry * math.copysign(abs(s) ** (2 / n), s)
        pts.append((cx + x * math.cos(r) - y * math.sin(r), cy + x * math.sin(r) + y * math.cos(r)))
    draw.polygon(pts, fill=fill)


def draw_icon():
    k = S / 512
    egg = egg_polygon(258 * k, 258 * k, 200 * k, 224 * k, 10)

    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).polygon(egg, fill=255)

    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    # soft drop shadow, down and to the right
    shadow = mask.filter(ImageFilter.GaussianBlur(6 * k))
    shadow = ImageChops.offset(shadow, int(4 * k), int(6 * k))
    img.paste((0, 0, 0, 90), (0, 0), shadow)

    # shell: light grey, shading to white towards the upper left
    shell = Image.new("RGBA", (S, S))
    sd = ImageDraw.Draw(shell)
    for y in range(S):
        t = y / S
        v = int(250 - 22 * t)
        sd.line([(0, y), (S, y)], fill=(v, v, v, 255))
    spots = ImageDraw.Draw(shell)
    blob(spots, 302 * k, 162 * k, 80 * k, 80 * k, 8, 0.45, (30, 240, 50, 255))
    blob(spots, 138 * k, 300 * k, 50 * k, 78 * k, 0, 0.0, (20, 225, 40, 255))
    blob(spots, 340 * k, 366 * k, 66 * k, 80 * k, 25, 0.1, (25, 232, 45, 255))
    img.paste(shell, (0, 0), mask)

    # thin darker rim
    edge = ImageChops.subtract(mask, mask.filter(ImageFilter.MinFilter(int(3 * k) | 1)))
    img.paste((200, 200, 200, 255), (0, 0), edge)
    return img.resize((512, 512), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    png = os.path.join(OUT, "app.png")
    if "--from-png" in sys.argv:
        img = Image.open(png).convert("RGBA")
    else:
        img = draw_icon()
        img.save(png)
    img.resize((256, 256), Image.LANCZOS).save(os.path.join(OUT, "app.ico"), sizes=ICO_SIZES)


if __name__ == "__main__":
    main()
