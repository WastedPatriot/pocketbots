"""Flat bot-picker marks.

Same shapes the firmware draws in firmware/src/mark.cpp. Numbers are
duplicated on purpose so a PNG you drop in matches the on-device family.
If you change a radius here, change it there too.

Coordinates are y-up, inside the unit box [-1, 1].
"""

from __future__ import annotations

import math

# Picker palette. Blue is the owner's #1E88FF. The rest are the flat
# swatches from the same dialog, named so bots.json can say "orange".
PALETTE = {
    "white": 0xF4F4F5,
    "brown": 0x8D6E63,
    "red": 0xE53935,
    "orange": 0xFB8C00,
    "amber": 0xFFB300,
    "green": 0x43A047,
    "teal": 0x00ACC1,
    "blue": 0x1E88FF,
    "purple": 0x8E24AA,
    "pink": 0xEC407A,
    "grey": 0x9E9E9E,
}

SHAPES = (
    "circle",
    "cloud",
    "squircle",
    "plus",
    "peanut",
    "blob",
    "flower",
    "drop",
    "pill",
    "triangle",
    "house",
    "star",
    "hexagon",
)

EYE = (0x16, 0x16, 0x1A)


def parse_color(value: str) -> int:
    value = value.strip().lower()
    if value in PALETTE:
        return PALETTE[value]
    if value.startswith("#"):
        value = value[1:]
    if len(value) != 6:
        raise ValueError(f"not a colour: {value}")
    return int(value, 16)


def rgb(value: int) -> tuple[int, int, int]:
    return (value >> 16) & 255, (value >> 8) & 255, value & 255


def _clamp(v: float, lo: float, hi: float) -> float:
    return lo if v < lo else hi if v > hi else v


def _dot(ax, ay, bx, by) -> float:
    return ax * bx + ay * by


def sd_circle(x, y, r) -> float:
    return math.hypot(x, y) - r


def sd_box(x, y, hx, hy) -> float:
    dx = abs(x) - hx
    dy = abs(y) - hy
    ax = dx if dx > 0 else 0.0
    ay = dy if dy > 0 else 0.0
    inside = dx if dx > dy else dy
    if inside > 0:
        inside = 0.0
    return math.hypot(ax, ay) + inside


def sd_roundbox(x, y, hx, hy, r) -> float:
    return sd_box(x, y, hx - r, hy - r) - r


def smin(a, b, k) -> float:
    h = _clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0)
    return (b * (1.0 - h) + a * h) - k * h * (1.0 - h)


def sd_poly(x, y, pts) -> float:
    d = (x - pts[0][0]) ** 2 + (y - pts[0][1]) ** 2
    s = 1.0
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        ex = pts[j][0] - pts[i][0]
        ey = pts[j][1] - pts[i][1]
        wx = x - pts[i][0]
        wy = y - pts[i][1]
        denom = ex * ex + ey * ey
        t = 0.0 if denom == 0 else _clamp((wx * ex + wy * ey) / denom, 0.0, 1.0)
        bx = wx - ex * t
        by = wy - ey * t
        d = min(d, bx * bx + by * by)
        c1 = y >= pts[i][1]
        c2 = y < pts[j][1]
        c3 = ex * wy > ey * wx
        if (c1 and c2 and c3) or ((not c1) and (not c2) and (not c3)):
            s = -s
    return s * math.sqrt(d)


def sd_round_poly(x, y, pts, rad) -> float:
    # Shrink towards the centroid, then expand by rad so corners round off.
    cx = sum(p[0] for p in pts) / len(pts)
    cy = sum(p[1] for p in pts) / len(pts)
    shrunk = []
    for px, py in pts:
        vx, vy = px - cx, py - cy
        length = math.hypot(vx, vy) or 1.0
        shrunk.append((px - vx / length * rad, py - vy / length * rad))
    return sd_poly(x, y, shrunk) - rad


def _ngon(n, r, rot) -> list[tuple[float, float]]:
    return [
        (math.cos(rot + i * 2 * math.pi / n) * r, math.sin(rot + i * 2 * math.pi / n) * r)
        for i in range(n)
    ]


def sdf(shape: str, x: float, y: float) -> float:
    if shape == "circle":
        return sd_circle(x, y, 0.78)
    if shape == "squircle":
        return sd_roundbox(x, y, 0.74, 0.74, 0.30)
    if shape == "pill":
        return sd_roundbox(x, y, 0.86, 0.50, 0.50)
    if shape == "plus":
        v = sd_roundbox(x, y, 0.30, 0.78, 0.30)
        h = sd_roundbox(x, y, 0.78, 0.30, 0.30)
        return smin(v, h, 0.08)
    if shape == "hexagon":
        # Vertex up, corners knocked back so it reads as the picker's
        # soft hex rather than a bolt head.
        return sd_round_poly(x, y, _ngon(6, 0.88, math.pi / 2), 0.26)
    if shape == "triangle":
        return sd_round_poly(x, y, _ngon(3, 0.92, math.pi / 2), 0.22)
    if shape == "house":
        pts = [(0.0, 0.86), (0.72, 0.18), (0.72, -0.78), (-0.72, -0.78), (-0.72, 0.18)]
        return sd_round_poly(x, y, pts, 0.16)
    if shape == "star":
        # Chubby four-point sparkle, not a shuriken.
        pts = []
        for i in range(8):
            a = math.pi / 2 + i * math.pi / 4
            rad = 0.86 if i % 2 == 0 else 0.40
            pts.append((math.cos(a) * rad, math.sin(a) * rad))
        return sd_round_poly(x, y, pts, 0.12)
    if shape == "peanut":
        a = sd_circle(x - 0.30, y, 0.50)
        b = sd_circle(x + 0.30, y, 0.50)
        return smin(a, b, 0.28)
    if shape == "cloud":
        # Flat-bottomed lump with three bumps. The picker cloud, more or less.
        d = sd_roundbox(x, y - 0.10, 0.62, 0.36, 0.32)
        d = smin(d, sd_circle(x - 0.30, y + 0.10, 0.34), 0.14)
        d = smin(d, sd_circle(x + 0.26, y + 0.14, 0.38), 0.14)
        d = smin(d, sd_circle(x - 0.02, y + 0.30, 0.28), 0.12)
        return d
    if shape == "blob":
        d = sd_circle(x - 0.06, y + 0.08, 0.50)
        d = smin(d, sd_circle(x + 0.30, y - 0.10, 0.42), 0.30)
        d = smin(d, sd_circle(x - 0.32, y - 0.16, 0.38), 0.28)
        d = smin(d, sd_circle(x + 0.02, y + 0.36, 0.30), 0.22)
        return d
    if shape == "flower":
        d = sd_circle(x, y, 0.34)
        for i in range(6):
            a = math.pi / 2 + i * math.pi / 3
            d = smin(d, sd_circle(x + math.cos(a) * 0.40, y + math.sin(a) * 0.40, 0.32), 0.10)
        return d
    if shape == "drop":
        body = sd_circle(x, y - 0.16, 0.52)
        tip = sd_poly(x, y, [(0.0, 0.88), (0.46, -0.02), (-0.46, -0.02)])
        return smin(body, tip, 0.20)
    raise KeyError(shape)


def render(shape: str, color: int, size: int = 168, placeholder: bool = False):
    """Return an RGBA bytes object, top row first."""
    from PIL import Image, ImageDraw, ImageFont

    r, g, b = rgb(color)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = img.load()
    # One pixel in unit space.
    step = 2.0 / size
    for py in range(size):
        # Pixel centre, y up.
        y = 1.0 - (py + 0.5) * step
        for x_i in range(size):
            x = -1.0 + (x_i + 0.5) * step
            d = sdf(shape, x, y)
            # One pixel of coverage. Flat fill, no halo: the picker marks
            # are solid colour. Idle "glow" is a scale pulse in the firmware.
            cover = _clamp(0.5 - d / step, 0.0, 1.0)
            if cover > 0:
                px[x_i, py] = (r, g, b, int(cover * 255 + 0.5))

    # Eyes. Same proportions as mark.cpp: two vertical capsules,
    # slightly above centre. Drawn in pixel space so they stay crisp.
    draw = ImageDraw.Draw(img)
    # Two dark vertical capsules, slightly above centre. Matches mark.cpp.
    ew, eh = int(size * 0.115), int(size * 0.205)
    cx, cy = size // 2, int(size * 0.455)
    spread = int(size * 0.105)
    for sx in (-1, 1):
        ex = cx + sx * spread - ew // 2
        ey = cy - eh // 2
        draw.rounded_rectangle([ex, ey, ex + ew - 1, ey + eh - 1], radius=ew // 2, fill=EYE + (255,))

    if placeholder:
        # A caption, not part of the mark. Sits in the margin so a
        # folder thumbnail is obviously not the finished avatar.
        try:
            font = ImageFont.truetype("DejaVuSans.ttf", max(9, size // 14))
        except OSError:
            font = ImageFont.load_default()
        label = "PLACEHOLDER"
        box = draw.textbbox((0, 0), label, font=font)
        tw, th = box[2] - box[0], box[3] - box[1]
        draw.text(((size - tw) / 2, size - th - 4), label, fill=(255, 255, 255, 170), font=font)
    return img
