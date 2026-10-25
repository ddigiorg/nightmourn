# ==============================================================================
# tools/gen_trees.py
#
# Generates the tree sprites in the style of Age of Empires II: DE's
# pre-rendered broadleaf trees (dark green, lit from the upper left, with a
# soft ground shadow to the lower right), and pastes them into the game atlas
# side by side (AtlasPixels::Trees in demos/01-player-armies).
#
# Each frame is 128x256 px, twice a unit frame (64x128), and is drawn 1x2
# tiles. The trunk's foot is the frame's pivot, 64 px above the bottom edge
# (a quarter of the height, the same as a unit's), which leaves room below it
# for the shadow.
#
# Each canopy is a few thousand small leaves scattered over clumps of leaves
# inside a rough ellipsoid, painted back to front. A leaf is shaded by its
# clump's surface normal blended with the canopy's, so clumps read as bumps on
# one big crown. Drawn 4x supersampled, then box filtered down. Deterministic
# (fixed seeds).
#
#   python3 tools/gen_trees.py                       # update assets/graphics/atlas.png
#   python3 tools/gen_trees.py --out trees.png       # sprites only, for a look
#
# Needs numpy and Pillow.
# ==============================================================================

import argparse
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

W, H  = 128, 256   # frame size (px)
FOOT  = (64, 192)  # trunk foot, the sprite's pivot (px)
S     = 4          # supersampling
LIGHT = np.array([-0.55, -0.65, 0.52])  # x right, y down, z toward the viewer
LIGHT = LIGHT / np.linalg.norm(LIGHT)

def hexc(h):
    return np.array([int(h[i:i+2], 16) for i in (1, 3, 5)], dtype=np.float64)

# Dark green leaves: deep shade -> mid -> sunlit tips, and a warmer highlight
LEAF_RAMP = [hexc("#0f1f0c"), hexc("#1e3816"), hexc("#2f5220"), hexc("#46712c"), hexc("#6a9140")]
BARK_LIT  = hexc("#7a5a3a")
BARK_DARK = hexc("#3a2616")
SHADOW    = (12, 22, 8)

def ramp(t):
    t = np.clip(t, 0.0, 1.0) * (len(LEAF_RAMP) - 1)
    i = min(int(t), len(LEAF_RAMP) - 2)
    f = t - i
    return LEAF_RAMP[i] * (1 - f) + LEAF_RAMP[i + 1] * f

def to_px(v):
    return tuple(int(round(c)) for c in np.clip(v, 0, 255))

# ------------------------------------------------------------------------------
# Tree
# ------------------------------------------------------------------------------

def make_tree(seed, crown_c, crown_r, trunk_h, lean):
    """crown_c: canopy center (px), crown_r: canopy radii (rx, ry, rz) in px."""
    rng = np.random.default_rng(seed)
    img  = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    fx, fy = FOOT

    # --- Ground shadow: a soft blob to the lower right of the foot, wider
    # than tall (the ground is foreshortened 2:1)
    shadow = Image.new("L", (W * S, H * S), 0)
    sd = ImageDraw.Draw(shadow)
    for _ in range(30):
        cx = fx + 14 + np.clip(rng.normal(0, 9), -14, 14)
        cy = fy + 5 + rng.normal(0, 4)
        rx = rng.uniform(16, 26)
        ry = rx * 0.48
        sd.ellipse([(cx - rx) * S, (cy - ry) * S, (cx + rx) * S, (cy + ry) * S], fill=255)
    shadow = shadow.filter(ImageFilter.GaussianBlur(3 * S))
    shadow_rgba = Image.new("RGBA", shadow.size, SHADOW + (0,))
    shadow_rgba.putalpha(shadow.point(lambda a: int(a * 0.42)))
    img.alpha_composite(shadow_rgba)

    # --- Trunk: a tapered, slightly leaning column with root flare, lit on
    # the left; bark streaks run up it
    top_y = fy - trunk_h
    for y in range(int(top_y * S), int((fy + 2) * S)):
        t  = (y / S - top_y) / (fy - top_y)  # 0 top, 1 foot
        cx = fx + lean * (1 - t) * 6
        hw = 4.0 + 2.6 * t + 4.5 * max(0.0, t - 0.82) ** 1.5 * 12  # flare at the roots
        for x in range(int((cx - hw) * S), int((cx + hw) * S) + 1):
            u = (x / S - cx) / hw  # -1 left .. 1 right
            if abs(u) > 1: continue
            lit = np.clip(0.75 - 0.6 * u - 0.25 * u * u, 0, 1)
            streak = 0.85 + 0.3 * np.sin(x * 0.9 / S * 3.1 + np.sin(y / S * 0.35) * 2)
            c = (BARK_DARK + (BARK_LIT - BARK_DARK) * lit) * streak
            # contact shadow at the foot
            if t > 0.93: c = c * (1 - 0.5 * (t - 0.93) / 0.07)
            img.putpixel((x, y), to_px(c) + (255,))

    # A few branches up into the crown
    for _ in range(4):
        x0, y0 = fx + lean * 2, top_y + rng.uniform(4, 14)
        ang = rng.uniform(-2.5, -0.6)
        L = rng.uniform(16, 26)
        x1, y1 = x0 + np.cos(ang) * L, y0 + np.sin(ang) * L
        draw.line([(x0 * S, y0 * S), (x1 * S, y1 * S)], fill=to_px(BARK_DARK * 1.1) + (255,), width=int(2.5 * S))

    # --- Canopy: clumps of leaves inside an ellipsoid
    cx, cy = crown_c
    rx, ry, rz = crown_r
    clumps = []
    for _ in range(60):
        # in the ellipsoid, biased outward so the crown is lumpy
        d = rng.normal(0, 1, 3); d /= np.linalg.norm(d)
        r = rng.uniform(0.2, 0.85) ** 0.5
        p = np.array([cx + d[0] * rx * r, cy + d[1] * ry * r, d[2] * rz * r])
        clumps.append((p, rng.uniform(10, 16)))

    # Deep shade inside the crown, so gaps between clumps show leaves in
    # shadow instead of the ground
    core = [(cx + rx * k * np.cos(a), cy + ry * k * np.sin(a))
            for a, k in zip(np.linspace(0, 2 * np.pi, 48, endpoint=False), rng.uniform(0.62, 0.74, 48))]
    draw.polygon([(x * S, y * S) for x, y in core], fill=to_px(LEAF_RAMP[0] * 1.2) + (255,))

    leaves = []
    for p, cr in clumps:
        for _ in range(int(cr * cr * 1.6)):
            n = rng.normal(0, 1, 3); n /= np.linalg.norm(n)
            if n[2] < -0.2: continue  # back of the clump is hidden anyway
            q = p + n * cr * rng.uniform(0.75, 1.0)
            # stay inside the crown's ellipsoid, a little ragged
            e = ((q[0] - cx) / rx) ** 2 + ((q[1] - cy) / ry) ** 2 + (q[2] / rz) ** 2
            if e > rng.uniform(1.0, 1.15): continue
            g = np.array([(q[0] - cx) / rx, (q[1] - cy) / ry, q[2] / rz])
            g /= np.linalg.norm(g) + 1e-9
            normal = 0.55 * n + 0.45 * g
            normal /= np.linalg.norm(normal)
            leaves.append((q, normal, g))

    leaves.sort(key=lambda l: l[0][2])  # back to front
    for q, normal, g in leaves:
        diff = max(0.0, float(normal @ LIGHT))
        # occlusion: leaves deep in the crown and on its underside are darker
        ao = np.clip(0.35 + 0.65 * (0.5 + 0.5 * g[2]), 0, 1) * np.clip(1.0 - 0.45 * max(0.0, g[1]), 0, 1)
        t = 0.08 + 0.92 * (0.25 + 0.75 * diff) * ao + rng.normal(0, 0.06)
        c = ramp(t)
        if t > 0.8: c = c * 0.85 + hexc("#8aa84a") * 0.15  # warm sunlit tips
        # a leaf: a small ellipse at a random angle
        lr = rng.uniform(1.6, 2.6)
        ang = rng.uniform(0, np.pi)
        pts = []
        for k in range(8):
            a = k / 8 * 2 * np.pi
            px = np.cos(a) * lr
            py = np.sin(a) * lr * 0.55
            pts.append(((q[0] + px * np.cos(ang) - py * np.sin(ang)) * S,
                        (q[1] + px * np.sin(ang) + py * np.cos(ang)) * S))
        draw.polygon(pts, fill=to_px(c) + (255,))

    # Down to frame size; premultiplied, so transparent pixels don't darken edges
    img = img.convert("RGBa").resize((W, H), Image.BOX).convert("RGBA")
    return bleed(fade_edges(img))

def fade_edges(img, margin=4):
    """Fade alpha to 0 over the outer `margin` px, so linear filtering at a
    frame's edge never picks up the neighboring frame (or this one's edge
    smeared into a line)."""
    a = np.asarray(img).astype(np.float64)
    wx = np.clip(np.minimum(np.arange(W), W - 1 - np.arange(W)) / margin, 0, 1)
    wy = np.clip(np.minimum(np.arange(H), H - 1 - np.arange(H)) / margin, 0, 1)
    a[..., 3] *= wy[:, None] * wx[None, :]
    return Image.fromarray(a.round().astype(np.uint8), "RGBA")

def bleed(img, steps=4):
    """Copy edge colors into transparent neighbors (alpha stays 0), so linear
    filtering at the sprite's edge never pulls in black."""
    a = np.asarray(img).astype(np.float64)
    rgb, alpha = a[..., :3].copy(), a[..., 3]
    known = alpha > 0
    for _ in range(steps):
        acc = np.zeros_like(rgb); cnt = np.zeros(alpha.shape)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            k = np.roll(known, (dy, dx), (0, 1))
            acc += np.roll(rgb, (dy, dx), (0, 1)) * k[..., None]
            cnt += k
        grow = (~known) & (cnt > 0)
        rgb[grow] = acc[grow] / cnt[grow][..., None]
        known = known | grow
    out = np.dstack([rgb, alpha]).astype(np.uint8)
    return Image.fromarray(out, "RGBA")

# Three looks, so a forest doesn't repeat: a broad oak, a taller one, a
# smaller, rounder one
VARIANTS = [
    dict(seed=11, crown_c=(63, 88), crown_r=(54, 56, 42), trunk_h=90, lean=-0.4),
    dict(seed=23, crown_c=(65, 80), crown_r=(46, 62, 38), trunk_h=96, lean=0.5),
    dict(seed=37, crown_c=(63, 102), crown_r=(52, 46, 40), trunk_h=76, lean=0.0),
]

parser = argparse.ArgumentParser()
parser.add_argument("--atlas", default="assets/graphics/atlas.png")
parser.add_argument("--at", type=int, nargs=2, default=(640, 0), metavar=("X", "Y"),
                    help="top-left pixel of the first tree frame in the atlas")
parser.add_argument("--out", help="write the sprites alone here instead of into the atlas")
args = parser.parse_args()

sheet = Image.new("RGBA", (W * len(VARIANTS), H), (0, 0, 0, 0))
for i, v in enumerate(VARIANTS):
    sheet.paste(make_tree(**v), (i * W, 0))

if args.out:
    sheet.save(args.out)
else:
    atlas = Image.open(args.atlas).convert("RGBA")
    x, y = args.at
    assert atlas.width >= x + sheet.width and atlas.height >= y + sheet.height
    atlas.paste(sheet, (x, y))
    atlas.save(args.atlas, optimize=True)
