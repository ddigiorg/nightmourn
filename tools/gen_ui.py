# ==============================================================================
# tools/gen_ui.py
#
# Generates the UI atlas, assets/graphics/ui.png: the player action bar's
# art (a stone eagle, the backplate, slot frames, menu buttons and their
# icons) and placeholder item icons. Everything is carved from cold, blue-
# grey moonlit stone: each piece is a height map, lit from the upper left,
# with stone grain and ambient occlusion in the creases.
#
# Painted at 2x (two pixels per UI point) so it stays crisp on 150% and 200%
# screens; the game draws it with density 2 (see GUI/UIAtlas.hpp, which
# lists every sprite's pixel box; keep the two in sync).
#
# Placeholder art: hand-drawn sprites will replace it, at the same boxes.
#
#   python3 tools/gen_ui.py                 # writes assets/graphics/ui.png
#   python3 tools/gen_ui.py --out ui.png    # somewhere else, for a look
#
# Needs numpy and Pillow. Deterministic (fixed seeds).
# ==============================================================================

import argparse
import math
import numpy as np
from PIL import Image, ImageDraw

ATLAS_W, ATLAS_H = 1312, 208

# Pixel boxes { x0, y0, x1, y1 }, matching GUI/UIAtlas.hpp
EAGLE   = (0, 0, 140, 120)          # 70 x 60 pt, crouched, faces right (inward on the left)
PLATE   = (148, 0, 1298, 100)       # 575 x 50 pt
SLOT    = (0, 128, 72, 200)         # 36 x 36 pt, socket under the icon
RIM     = (80, 128, 152, 200)       # 36 x 36 pt, thin bevelled rim over the icon
BUTTON  = (160, 128, 204, 192)      # 22 x 32 pt, rectangular menu button
DIVIDER = (212, 128, 228, 200)      # 8 x 36 pt
FILL    = (236, 128, 252, 142)      # 8 x 7 pt, hp bar fill (white, tinted in game)
MENU_ICONS_AT = (260, 128)          # 7 x (40 x 40 px), 48 px apart
ITEM_ICONS_AT = (604, 128)          # 4 x (72 x 72 px), 80 px apart
MENU_ICONS = ["skills", "equipment", "inventory", "items", "options", "debug", "main"]
ITEMS      = ["wood", "stone", "gold", "food"]

LIGHT = np.array([-0.55, -0.70, 0.45])
LIGHT = LIGHT / np.linalg.norm(LIGHT)


def hexc(h):
    return np.array([int(h[i:i + 2], 16) for i in (1, 3, 5)], dtype=np.float64) / 255.0


# Cold stone, deep shadow to moonlit highlight
STONE = [hexc("#141a23"), hexc("#28323f"), hexc("#435265"), hexc("#687d94"),
         hexc("#94aac0"), hexc("#c9d8e6")]


# ------------------------------------------------------------------------------
# Helpers
# ------------------------------------------------------------------------------

def grid(w, h):
    y, x = np.mgrid[0:h, 0:w].astype(np.float64)
    return x + 0.5, y + 0.5


def box_blur(a, r):
    """Box blur of radius r (pixels), edge-clamped."""
    if r < 1:
        return a
    r = int(r)
    k = 2 * r + 1
    p = np.pad(a, r, mode="edge")
    c = np.cumsum(p, axis=0)
    c = np.vstack([np.zeros((1, c.shape[1])), c])
    a = (c[k:] - c[:-k]) / k
    c = np.cumsum(a, axis=1)
    c = np.hstack([np.zeros((c.shape[0], 1)), c])
    return (c[:, k:] - c[:, :-k]) / k


def blur(a, r):
    """Roughly Gaussian: three box blurs."""
    r = max(1, int(round(r / 1.7)))
    for _ in range(3):
        a = box_blur(a, r)
    return a


def value_noise(w, h, cell, seed, octaves=4):
    rng = np.random.default_rng(seed)
    out = np.zeros((h, w))
    amp, total = 1.0, 0.0
    for o in range(octaves):
        c = max(1, cell / (2 ** o))
        gw, gh = int(w / c) + 2, int(h / c) + 2
        g = rng.random((gh, gw)).astype(np.float32)
        img = Image.fromarray(g, mode="F").resize((int(gw * c), int(gh * c)), Image.BILINEAR)
        out += amp * np.asarray(img)[:h, :w]
        total += amp
        amp *= 0.5
    return out / total


def poly_mask(w, h, pts, s=1):
    img = Image.new("L", (w * 4, h * 4), 0)
    ImageDraw.Draw(img).polygon([(x * 4 * s, y * 4 * s) for x, y in pts], fill=255)
    return np.asarray(img.resize((w, h), Image.BOX), dtype=np.float64) / 255.0


def ellipse_mask(x, y, cx, cy, rx, ry, angle=0.0):
    ca, sa = math.cos(angle), math.sin(angle)
    dx, dy = x - cx, y - cy
    u = (dx * ca + dy * sa) / rx
    v = (-dx * sa + dy * ca) / ry
    d = (np.sqrt(u * u + v * v) - 1.0) * min(rx, ry)
    return np.clip(0.5 - d, 0.0, 1.0)


def capsule_mask(x, y, p0, p1, r0, r1):
    ax, ay = p0
    bx, by = p1
    pax, pay = x - ax, y - ay
    bax, bay = bx - ax, by - ay
    t = np.clip((pax * bax + pay * bay) / (bax * bax + bay * bay), 0.0, 1.0)
    d = np.hypot(pax - bax * t, pay - bay * t) - (r0 + (r1 - r0) * t)
    return np.clip(0.5 - d, 0.0, 1.0)


def round_rect_mask(x, y, x0, y0, x1, y1, r):
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    hx, hy = (x1 - x0) / 2 - r, (y1 - y0) / 2 - r
    qx, qy = np.abs(x - cx) - hx, np.abs(y - cy) - hy
    d = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r
    return np.clip(0.5 - d, 0.0, 1.0)


def pillow(mask, r):
    """Height that rises from 0 at the mask's edge to ~1 inside, over r px."""
    b = blur(mask, r) * mask
    return np.sqrt(np.clip((b - 0.0) / 0.5, 0.0, 1.0))


def ramp(t, colors):
    t = np.clip(t, 0.0, 1.0) * (len(colors) - 1)
    i = np.minimum(t.astype(int), len(colors) - 2)
    f = (t - i)[..., None]
    c = np.array(colors)
    return c[i] * (1 - f) + c[i + 1] * f


def shade_stone(height, alpha, seed, relief=6.0, grain=0.08, base=0.5, contrast=0.55,
                ao_radius=6, ao_strength=1.4, rim=0.25):
    """Height map -> RGBA float image of carved stone."""
    h, w = height.shape
    gy, gx = np.gradient(height)
    n = np.dstack([-gx * relief, -gy * relief, np.ones_like(height)])
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    lam = np.clip(n @ LIGHT, 0.0, 1.0)

    # Ambient occlusion: darker where the surroundings stand higher
    ao = np.clip(1.0 - ao_strength * np.maximum(blur(height, ao_radius) - height, 0.0), 0.35, 1.0)

    noise = value_noise(w, h, 24, seed) - 0.5
    speck = value_noise(w, h, 2, seed + 1, octaves=2) - 0.5
    t = (base + contrast * (lam - 0.55)) * ao + grain * noise + 0.05 * speck
    # Moonlit rim on surfaces that face up and left
    t += rim * np.clip(n[..., 0] * -0.6 + n[..., 1] * -0.8 - 0.55, 0.0, 1.0)

    rgb = ramp(t, STONE)
    return np.dstack([rgb, alpha])


def to_image(rgba, s=1):
    """Float RGBA (straight alpha) -> PIL image, box-downsampled by s."""
    a = rgba[..., 3:4]
    pm = np.dstack([rgba[..., :3] * a, a])          # premultiply to downsample
    img = Image.fromarray((np.clip(pm, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA")
    if s > 1:
        img = img.resize((img.width // s, img.height // s), Image.BOX)
    p = np.asarray(img, dtype=np.float64) / 255.0
    a = p[..., 3:4]
    rgb = np.where(a > 0, p[..., :3] / np.maximum(a, 1e-6), 0.0)
    out = np.dstack([rgb, a])
    return Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA")


def outline(rgba, alpha, width, color=(0.04, 0.06, 0.09), strength=0.85):
    """Darken the outermost `width` px inside the shape, for a crisp edge."""
    inner = blur(alpha, width)
    edge = np.clip((1.0 - inner) * 2.0, 0.0, 1.0) * alpha * strength
    rgba[..., :3] = rgba[..., :3] * (1 - edge[..., None]) + np.array(color) * edge[..., None]
    return rgba


# ------------------------------------------------------------------------------
# Eagle
#
# A stone eagle crouched on a low plinth like a gargoyle, facing right: wings
# folded, shoulders hunched, head thrust forward over its talons, tail down
# behind it. Only a little taller than the bar it guards. Coordinates are in
# 1x frame pixels (140 x 120) and scaled by S for supersampling.
# ------------------------------------------------------------------------------

def gen_eagle():
    S = 4
    W, H = 140 * S, 120 * S
    x, y = grid(W, H)
    x /= S
    y /= S

    height = np.zeros((H, W))
    alpha  = np.zeros((H, W))

    def add(mask, base, amp, r):
        nonlocal height, alpha
        hm = base + amp * pillow(mask, r * S)
        height = np.where(mask > 0.5, np.maximum(height, hm), height)
        alpha = np.maximum(alpha, mask)

    # Plinth: a low block along the bottom, running under the bar's end
    add(round_rect_mask(x, y, 14, 100, 140, 120, 3), 0.10, 0.30, 4)
    add(round_rect_mask(x, y, 8, 92, 140, 104, 3), 0.20, 0.35, 3)

    # Tail: feathers fanned down behind the plinth
    for k in range(4):
        ang = math.radians(128 + k * 12)
        p0 = (40, 74)
        p1 = (40 + 34 * math.cos(ang), 74 + 34 * math.sin(ang))
        add(capsule_mask(x, y, p0, p1, 5, 6), 0.22 + 0.01 * k, 0.26, 3)

    # Body: hunched, chest forward
    add(ellipse_mask(x, y, 66, 62, 36, 26, math.radians(-18)), 0.40, 0.50, 16)

    # Folded wing over the back: a raised shoulder, then long feathers
    # sweeping down to the tail
    add(ellipse_mask(x, y, 62, 40, 26, 15, math.radians(-12)), 0.60, 0.30, 8)
    wing = poly_mask(W, H, [(88, 36), (60, 26), (36, 36), (18, 58), (8, 84), (22, 86),
                            (48, 72), (80, 58)], S)
    add(wing, 0.66, 0.26, 6)
    for k in range(5):
        t = k / 4
        p0 = (70 - 18 * t, 40 + 6 * t)
        p1 = (14 + 14 * t, 82 - 6 * t)
        add(capsule_mask(x, y, p0, p1, 5.5, 3.0) * wing, 0.70 + 0.025 * k, 0.20, 3)
    # Scalloped coverts on the shoulder
    for k in range(4):
        c = (78 - 12 * k, 38 + 4 * k)
        add(ellipse_mask(x, y, c[0], c[1], 9, 6, math.radians(-30)) *
            (ellipse_mask(x, y, 62, 40, 26, 15, math.radians(-12)) > 0.5), 0.86 + 0.01 * k, 0.12, 2)

    # Legs and talons gripping the plinth's edge
    add(ellipse_mask(x, y, 96, 80, 14, 11, math.radians(-15)), 0.55, 0.30, 5)
    for tx in (94, 106, 118):
        add(capsule_mask(x, y, (tx - 5, 88), (tx + 3, 95), 3.6, 2.2), 0.60, 0.22, 2)

    # Neck thrust forward, the head low and level with the shoulders
    add(ellipse_mask(x, y, 98, 34, 15, 17, math.radians(40)), 0.62, 0.38, 9)
    for k in range(4):
        b = (96 - 4 * k, 18 + 6 * k)
        add(capsule_mask(x, y, b, (b[0] - 11, b[1] + 8), 4.5, 1.8), 0.66 + 0.02 * k, 0.18, 2)
    add(ellipse_mask(x, y, 110, 17, 15, 11, math.radians(-4)), 0.76, 0.36, 8)
    beak = poly_mask(W, H, [(117, 9), (128, 8), (136, 13), (139, 21), (137, 29), (133, 33),
                            (131, 25), (125, 22), (118, 25)], S)
    add(beak, 0.84, 0.30, 3)
    add(capsule_mask(x, y, (106, 9), (121, 11), 3.0, 2.2), 1.02, 0.10, 2)  # brow
    height -= 0.30 * ellipse_mask(x, y, 116, 15, 2.4, 2.1)                  # eye
    height -= 0.12 * capsule_mask(x, y, (116, 21), (131, 21), 0.8, 0.6)     # gape

    # Feather texture: rows of small scales down the chest
    chest = (ellipse_mask(x, y, 66, 62, 36, 26, math.radians(-18)) > 0.5) & (wing < 0.5)
    rows = (y + 2.5 * np.sin(x * 0.6)) / 5.0
    height -= 0.05 * (rows - np.floor(rows)) * chest

    height = blur(height, 0.8 * S) * 0.5 + height * 0.5

    rgba = shade_stone(height, alpha, seed=11, relief=13.0 * S, grain=0.10, base=0.46,
                       contrast=0.95, ao_radius=4 * S, ao_strength=3.0)
    glint = ellipse_mask(x, y, 115.4, 14.2, 0.8, 0.7)
    rgba[..., :3] = rgba[..., :3] * (1 - glint[..., None]) + hexc("#d8ecff") * glint[..., None]
    rgba = outline(rgba, alpha, 1.2 * S)
    return to_image(rgba, S)


# ------------------------------------------------------------------------------
# Backplate: a dark stone strip with a thin bevelled rim. Almost all of it is
# covered by the bar, the slots and the buttons, so it only shows as the
# narrow seams between them.
# ------------------------------------------------------------------------------

def gen_plate():
    w, h = PLATE[2] - PLATE[0], PLATE[3] - PLATE[1]
    x, y = grid(w, h)
    slab = round_rect_mask(x, y, 0, 0, w, h, 5)
    height = 0.30 + 0.30 * pillow(slab, 4)
    # Seam between the hp bar and the slot row
    height -= 0.10 * round_rect_mask(x, y, 6, 19, w - 6, 21, 1)
    rgba = shade_stone(height, slab, seed=21, relief=12.0, grain=0.10, base=0.22,
                       contrast=0.55, ao_radius=4, ao_strength=1.2, rim=0.30)
    rgba = outline(rgba, slab, 1.2)
    return to_image(rgba)


# ------------------------------------------------------------------------------
# Slots, buttons, divider: thin frames, so the icons take nearly all the room
# ------------------------------------------------------------------------------

def gen_slot():
    s = SLOT[2] - SLOT[0]
    x, y = grid(s, s)
    outer = round_rect_mask(x, y, 0, 0, s, s, 3)
    inner = round_rect_mask(x, y, 4, 4, s - 4, s - 4, 3)
    height = 0.5 * pillow(outer, 2) - 0.4 * pillow(inner, 2)
    rgba = shade_stone(height, outer, seed=31, relief=10.0, grain=0.05, base=0.30,
                       contrast=0.6, ao_radius=3, ao_strength=1.0)
    hollow = inner[..., None]
    depth = np.clip((y - 4) / (s - 8), 0, 1)[..., None]
    socket = hexc("#06080d") * (1 - depth) + hexc("#0f151f") * depth
    rgba[..., :3] = rgba[..., :3] * (1 - hollow) + socket * hollow
    return to_image(rgba)


def gen_rim():
    s = RIM[2] - RIM[0]
    x, y = grid(s, s)
    outer = round_rect_mask(x, y, 0, 0, s, s, 3)
    inner = round_rect_mask(x, y, 4, 4, s - 4, s - 4, 3)
    ring = np.clip(outer - inner, 0, 1)
    height = 0.6 * pillow(outer, 2) * ring
    rgba = shade_stone(height, ring, seed=32, relief=16.0, grain=0.05, base=0.36,
                       contrast=0.8, ao_radius=2, ao_strength=0.6, rim=0.35)
    shadow = np.clip(blur(1.0 - inner, 2) * inner, 0, 1) * 0.7
    rgba[..., 3] = np.maximum(rgba[..., 3], shadow)
    rgba[..., :3] = np.where((ring < 0.5)[..., None], 0.0, rgba[..., :3])
    return to_image(rgba)


def gen_button():
    w, h = BUTTON[2] - BUTTON[0], BUTTON[3] - BUTTON[1]
    x, y = grid(w, h)
    outer = round_rect_mask(x, y, 0, 0, w, h, 4)
    face  = round_rect_mask(x, y, 4, 4, w - 4, h - 4, 3)
    height = 0.5 * pillow(outer, 2) - 0.25 * pillow(face, 3)
    rgba = shade_stone(height, outer, seed=41, relief=12.0, grain=0.06, base=0.32,
                       contrast=0.7, ao_radius=3, ao_strength=1.0, rim=0.3)
    # A dark face, lit faintly from the top, for the icon to sit on
    depth = np.clip((y - 4) / (h - 8), 0, 1)[..., None]
    face_c = hexc("#1a2230") * (1 - depth) + hexc("#0b1018") * depth
    rgba[..., :3] = rgba[..., :3] * (1 - face[..., None]) + face_c * face[..., None]
    rgba = outline(rgba, outer, 1.0)
    return to_image(rgba)


def gen_divider():
    w, h = DIVIDER[2] - DIVIDER[0], DIVIDER[3] - DIVIDER[1]
    x, y = grid(w, h)
    # A carved diamond on a thin pillar between the slots and the buttons
    shaft = round_rect_mask(x, y, w / 2 - 2, 4, w / 2 + 2, h - 4, 2)
    gem = poly_mask(w, h, [(w / 2, h / 2 - 9), (w / 2 + 6, h / 2), (w / 2, h / 2 + 9), (w / 2 - 6, h / 2)])
    alpha = np.maximum(shaft, gem)
    height = np.maximum(0.4 * pillow(shaft, 2), 0.6 * pillow(gem, 3))
    rgba = shade_stone(height, alpha, seed=51, relief=10.0, grain=0.05, base=0.45, contrast=0.7,
                       ao_radius=2, ao_strength=0.6, rim=0.35)
    rgba = outline(rgba, alpha, 1.0)
    return to_image(rgba)


def gen_fill():
    w, h = FILL[2] - FILL[0], FILL[3] - FILL[1]
    _, y = grid(w, h)
    t = y / h
    # White, lit from above: a bright top edge, a soft band, darker below
    v = 0.80 + 0.20 * np.exp(-((t - 0.25) / 0.15) ** 2) - 0.30 * t
    v = np.where(t < 1.5 / h, 1.0, v)
    rgba = np.dstack([v, v, v, np.ones_like(v)])
    return to_image(rgba)


# ------------------------------------------------------------------------------
# Menu icons: pale single-color glyphs (tinted in game), 40 x 40 px
# ------------------------------------------------------------------------------

def gen_menu_icon(name):
    S = 4
    s = 40 * S
    img = Image.new("L", (s, s), 0)
    d = ImageDraw.Draw(img)

    def P(*pts):
        return [(px * S, py * S) for px, py in pts]

    def line(pts, width):
        d.line(P(*pts), fill=255, width=int(width * S), joint="curve")

    if name == "skills":        # open book
        d.polygon(P((4, 10), (19, 13), (19, 33), (4, 30)), fill=255)
        d.polygon(P((21, 13), (36, 10), (36, 30), (21, 33)), fill=255)
        for k in range(3):
            line([(7, 16 + 4 * k), (16, 18 + 4 * k)], 1.2)
            line([(24, 18 + 4 * k), (33, 16 + 4 * k)], 1.2)
    elif name == "equipment":   # helmet
        d.pieslice(P((7, 6), (33, 38)), 180, 360, fill=255)
        d.rectangle(P((7, 21), (33, 30)), fill=255)
        d.rectangle(P((18, 6), (22, 31)), fill=0)
        d.rectangle(P((10, 18), (30, 21)), fill=0)
        d.rectangle(P((19, 7), (21, 34)), fill=255)
    elif name == "inventory":   # bag
        d.ellipse(P((7, 13), (33, 37)), fill=255)
        d.polygon(P((13, 14), (27, 14), (24, 9), (16, 9)), fill=255)
        d.ellipse(P((14, 3), (26, 11)), outline=255, width=2 * S)
        line([(12, 17), (28, 17)], 1.6)
    elif name == "items":       # crossed sword and scroll: an item codex
        d.rounded_rectangle(P((8, 6), (32, 34)), radius=4 * S, fill=255)
        d.rounded_rectangle(P((11, 9), (29, 31)), radius=2 * S, fill=0)
        for k in range(4):
            line([(14, 14 + 4.5 * k), (26, 14 + 4.5 * k)], 1.4)
    elif name == "options":     # gear
        cx, cy = 20 * S, 20 * S
        pts = []
        for k in range(16):
            r = (15 if k % 2 == 0 else 11.5) * S
            a = math.radians(k * 22.5)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
        d.polygon(pts, fill=255)
        d.ellipse((cx - 5 * S, cy - 5 * S, cx + 5 * S, cy + 5 * S), fill=0)
    elif name == "debug":       # bug
        d.ellipse(P((12, 12), (28, 34)), fill=255)
        d.ellipse(P((15, 6), (25, 14)), fill=255)
        line([(20, 14), (20, 33)], 1.2)
        for k, yy in enumerate((17, 23, 29)):
            line([(12, yy), (5, yy - 3 + 2 * k)], 1.8)
            line([(28, yy), (35, yy - 3 + 2 * k)], 1.8)
        line([(17, 7), (13, 3)], 1.4)
        line([(23, 7), (27, 3)], 1.4)
        d.line(P((20, 14), (20, 33)), fill=0, width=int(1.2 * S))
    elif name == "main":        # moon over a gate: the main menu
        d.ellipse(P((8, 4), (24, 20)), fill=255)
        d.ellipse(P((13, 2), (28, 17)), fill=0)
        d.rectangle(P((8, 22), (32, 36)), fill=255)
        d.pieslice(P((14, 24), (26, 38)), 180, 360, fill=0)
        d.rectangle(P((14, 31), (26, 36)), fill=0)

    a = np.asarray(img.resize((40, 40), Image.BOX), dtype=np.float64) / 255.0
    # A soft dark halo so the glyph reads on stone
    halo = np.clip(blur(a, 2) * 1.6, 0, 1) * 0.55
    alpha = np.maximum(a, halo)
    rgb = np.where(a[..., None] > 0.02, 1.0, 0.0) * (a / np.maximum(alpha, 1e-6))[..., None]
    rgba = np.dstack([np.repeat(rgb, 3, axis=2) if rgb.shape[2] == 1 else rgb, alpha])
    return to_image(rgba)


# ------------------------------------------------------------------------------
# Item icons: full-bleed square paintings, WoW style, 72 x 72 px
# ------------------------------------------------------------------------------

def gen_item_icon(name):
    S = 3
    s = 72 * S
    x, y = grid(s, s)
    x /= S
    y /= S
    nx, ny = (x - 36) / 36, (y - 36) / 36

    def lit(mask, base, light, dark, r=8):
        """A rounded object of `mask`, shaded from `dark` to `light`."""
        hm = pillow(mask, r * S)
        gy, gx = np.gradient(hm)
        n = np.dstack([-gx * 18 * S, -gy * 18 * S, np.ones_like(hm)])
        n /= np.linalg.norm(n, axis=2, keepdims=True)
        lam = np.clip(n @ LIGHT, 0, 1)[..., None]
        col = dark * (1 - lam) + light * lam
        col = col * 0.6 + base * 0.4 * (0.6 + 0.4 * lam)
        return col

    noise = value_noise(s, s, 10 * S, seed=hash(name) % 1000) - 0.5
    bg_c = {"wood": "#2a1a0e", "stone": "#1b2028", "gold": "#2a210a", "food": "#2a0f0e"}[name]
    vignette = np.clip(1.0 - 0.55 * (nx * nx + ny * ny), 0.25, 1)[..., None]
    rgb = hexc(bg_c) * vignette * (1 + 0.3 * noise[..., None])
    rgb = rgb + hexc("#2c3a4a") * 0.25 * np.clip(-ny, 0, 1)[..., None]

    def over(col, mask):
        nonlocal rgb
        m = mask[..., None]
        rgb = rgb * (1 - m) + col * m

    if name == "wood":
        # Two stacked logs, cut ends facing us
        for (cx, cy, r) in [(24, 44, 15), (48, 44, 15), (36, 22, 15)]:
            bark = ellipse_mask(x, y, cx, cy, r, r) * S
            bark = np.clip(bark, 0, 1)
            over(lit(bark, hexc("#5a3a1e"), hexc("#8a5a2e"), hexc("#2a160a"), 6), bark)
            end = np.clip(ellipse_mask(x, y, cx + 1, cy - 1, r - 4, r - 4) * S, 0, 1)
            d = np.hypot(x - cx - 1, y - cy + 1)
            rings = 0.5 + 0.5 * np.sin(d * 1.6 + noise * 3)
            col = hexc("#c4955a") * (0.8 + 0.2 * rings[..., None]) * (1 - 0.25 * (d / r))[..., None]
            over(col, end)
    elif name == "stone":
        rock = poly_mask(s, s, [(12, 50), (18, 28), (32, 14), (52, 18), (62, 36), (58, 56), (36, 62)], S)
        col = lit(rock, hexc("#7d8a99"), hexc("#c3ced9"), hexc("#2c333d"), 10)
        col = col * (1 + 0.25 * noise[..., None])
        over(col, rock)
        crack = capsule_mask(x, y, (34, 20), (40, 40), 0.9, 0.6) * rock
        over(hexc("#1d232b"), crack)
    elif name == "gold":
        for (cx, cy) in [(24, 50), (46, 52), (36, 40), (26, 30), (48, 32), (37, 20)]:
            coin = np.clip(ellipse_mask(x, y, cx, cy, 13, 9) * S, 0, 1)
            over(lit(coin, hexc("#d9a520"), hexc("#ffe680"), hexc("#6a4a08"), 4), coin)
            face = np.clip(ellipse_mask(x, y, cx, cy - 1, 8, 5) * S, 0, 1)
            over(hexc("#f0c040") * 0.9, face * 0.6)
    elif name == "food":
        apple = np.clip(np.maximum(ellipse_mask(x, y, 30, 42, 17, 18), ellipse_mask(x, y, 44, 42, 17, 18)) * S, 0, 1)
        over(lit(apple, hexc("#a8201a"), hexc("#ff6a50"), hexc("#3a0806"), 10), apple)
        stem = capsule_mask(x, y, (37, 26), (40, 14), 2, 1.5)
        over(hexc("#4a2e14"), stem)
        leaf = np.clip(ellipse_mask(x, y, 47, 18, 9, 4, math.radians(-25)) * S, 0, 1)
        over(lit(leaf, hexc("#3a7a2a"), hexc("#7acb50"), hexc("#163010"), 3), leaf)
        shine = np.clip(ellipse_mask(x, y, 25, 34, 4, 6, math.radians(30)) * S, 0, 1)
        over(hexc("#ffd0c0"), shine * 0.6)

    # Bevel: a lighter top-left edge and darker bottom-right, like a WoW icon
    e = 2.0
    bevel = np.clip(1 - np.minimum(np.minimum(x, y), np.minimum(72 - x, 72 - y)) / e, 0, 1)
    tl = ((x + y) < 72)[..., None]
    rgb = rgb * (1 - bevel[..., None] * 0.5) + np.where(tl, 0.55, 0.0) * bevel[..., None] * 0.5
    rgba = np.dstack([np.clip(rgb, 0, 1), np.ones((s, s))])
    return to_image(rgba, S)


# ------------------------------------------------------------------------------
# Main
# ------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="assets/graphics/ui.png")
    args = ap.parse_args()

    atlas = Image.new("RGBA", (ATLAS_W, ATLAS_H), (0, 0, 0, 0))

    def put(img, box):
        assert img.size == (box[2] - box[0], box[3] - box[1]), (img.size, box)
        atlas.paste(img, box[:2])

    put(gen_eagle(), EAGLE)
    put(gen_plate(), PLATE)
    put(gen_slot(), SLOT)
    put(gen_rim(), RIM)
    put(gen_button(), BUTTON)
    put(gen_divider(), DIVIDER)
    put(gen_fill(), FILL)
    for k, name in enumerate(MENU_ICONS):
        x0 = MENU_ICONS_AT[0] + 48 * k
        put(gen_menu_icon(name), (x0, MENU_ICONS_AT[1], x0 + 40, MENU_ICONS_AT[1] + 40))
    for k, name in enumerate(ITEMS):
        x0 = ITEM_ICONS_AT[0] + 80 * k
        put(gen_item_icon(name), (x0, ITEM_ICONS_AT[1], x0 + 72, ITEM_ICONS_AT[1] + 72))

    atlas.save(args.out)
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
