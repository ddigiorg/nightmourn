# ==============================================================================
# tools/gen_trees.py
#
# Generates the tree sprites in the style of Age of Empires II: DE's
# pre-rendered trees (dark green, lit from the upper left, with a soft ground
# shadow to the lower right), and pastes them into the game atlas side by
# side (AtlasPixels::Trees in demos/01-player-armies): two broadleaf oaks,
# then a pine.
#
# Each frame is 256x512 px, four times a unit frame (64x128) on each side,
# and is drawn 2x4 tiles. The trunk's foot is the frame's pivot, 128 px above
# the bottom edge (a quarter of the height, the same as a unit's), which
# leaves room below it for the shadow.
#
# Canopies are thousands of small leaves (oak) or needle strokes (pine)
# scattered over a crown, painted back to front:
#   oak   clumps of leaves inside a rough ellipsoid. A leaf is shaded by its
#         clump's surface normal blended with the crown's, so clumps read as
#         bumps on one big crown.
#   pine  stacked, drooping cone skirts that narrow to a point. Each skirt is
#         darker near the trunk, in the shadow of the one above it.
# Drawn 2x supersampled, then box filtered down. Deterministic (fixed seeds).
#
#   python3 tools/gen_trees.py                       # update assets/graphics/atlas.png
#   python3 tools/gen_trees.py --out trees.png       # sprites only, for a look
#
# Needs numpy and Pillow.
# ==============================================================================

import argparse
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

W, H  = 256, 512   # frame size (px)
FOOT  = (128, 384) # trunk foot, the sprite's pivot (px)
S     = 2          # supersampling
LIGHT = np.array([-0.55, -0.65, 0.52])  # x right, y down, z toward the viewer
LIGHT = LIGHT / np.linalg.norm(LIGHT)

def hexc(h):
    return np.array([int(h[i:i+2], 16) for i in (1, 3, 5)], dtype=np.float64)

# Oak leaves: deep shade -> mid -> sunlit tips. Darker than the light meadow
# so trees stand out from it.
OAK_RAMP  = [hexc("#08130a"), hexc("#11250e"), hexc("#1c3814"), hexc("#2b4f1e"), hexc("#44692c")]
OAK_TIP   = hexc("#64813a")
# Pine needles: bluer and darker than the oak
PINE_RAMP = [hexc("#06110d"), hexc("#0c2016"), hexc("#14321f"), hexc("#21472b"), hexc("#355f3a")]
PINE_TIP  = hexc("#4f7646")
BARK_LIT  = hexc("#5a3d25")
BARK_DARK = hexc("#22150b")
SHADOW    = (10, 18, 6)

def ramp(colors, t):
    t = np.clip(t, 0.0, 1.0) * (len(colors) - 1)
    i = min(int(t), len(colors) - 2)
    f = t - i
    return colors[i] * (1 - f) + colors[i + 1] * f

def to_px(v):
    return tuple(int(round(c)) for c in np.clip(v, 0, 255))

def shade(normal, ao, rng):
    """Light intensity in [0, 1] for a leaf facing `normal`, darkened by `ao`."""
    diff = max(0.0, float(normal @ LIGHT))
    return 0.08 + 0.92 * (0.25 + 0.75 * diff) * ao + rng.normal(0, 0.06)

# ------------------------------------------------------------------------------
# Shared Parts
# ------------------------------------------------------------------------------

def new_canvas():
    img = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)

def ground_shadow(img, rng, width):
    """A soft blob to the lower right of the foot, wider than tall (the
    ground is foreshortened 2:1). `width`: rough radius in px."""
    fx, fy = FOOT
    mask = Image.new("L", img.size, 0)
    d = ImageDraw.Draw(mask)
    for _ in range(40):
        cx = fx + width * 0.3 + np.clip(rng.normal(0, width * 0.18), -width * 0.3, width * 0.3)
        cy = fy + 10 + rng.normal(0, 6)
        rx = rng.uniform(0.32, 0.5) * width
        ry = rx * 0.48
        d.ellipse([(cx - rx) * S, (cy - ry) * S, (cx + rx) * S, (cy + ry) * S], fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(5 * S))
    layer = Image.new("RGBA", img.size, SHADOW + (0,))
    layer.putalpha(mask.point(lambda a: int(a * 0.45)))
    img.alpha_composite(layer)

def trunk(img, top_y, half_width, lean, flare):
    """A tapered column from `top_y` to the foot, lit on the left, with bark
    streaks and a root flare. `half_width(t)` for t 0 (top) .. 1 (foot)."""
    fx, fy = FOOT
    ys = np.arange(int(top_y * S), int((fy + 2) * S))
    xs = np.arange(W * S)
    X, Y = np.meshgrid(xs / S, ys / S)
    t  = (Y - top_y) / (fy - top_y)
    cx = fx + lean * (1 - t) * 10
    hw = half_width(t) + flare * np.maximum(0.0, t - 0.85) ** 1.5 * 40
    u  = (X - cx) / hw
    inside = np.abs(u) <= 1
    lit    = np.clip(0.75 - 0.6 * u - 0.25 * u * u, 0, 1)
    streak = 0.82 + 0.3 * np.sin(X * 1.4 + np.sin(Y * 0.18) * 2.5) * (0.6 + 0.4 * np.sin(Y * 0.05))
    col = (BARK_DARK + (BARK_LIT - BARK_DARK) * lit[..., None]) * streak[..., None]
    col = col * (1 - 0.5 * np.clip((t - 0.95) / 0.05, 0, 1))[..., None]  # contact shadow
    a = np.asarray(img).copy()
    region = a[ys[0]:ys[-1] + 1]
    region[inside, :3] = np.clip(col[inside], 0, 255).astype(np.uint8)
    region[inside, 3]  = 255
    a[ys[0]:ys[-1] + 1] = region
    return Image.fromarray(a, "RGBA")

def finish(img):
    # Down to frame size; premultiplied, so transparent pixels don't darken edges
    img = img.convert("RGBa").resize((W, H), Image.BOX).convert("RGBA")
    return bleed(fade_edges(img))

def fade_edges(img, margin=4):
    """Fade alpha to 0 over the outer `margin` px, so linear filtering at a
    frame's edge never picks up the neighboring frame."""
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
    return Image.fromarray(np.dstack([rgb, alpha]).astype(np.uint8), "RGBA")

def leaf(draw, q, size, ang, color, aspect=0.55):
    """A small ellipse at view position q (px)."""
    pts = []
    for k in range(8):
        a = k / 8 * 2 * np.pi
        px, py = np.cos(a) * size, np.sin(a) * size * aspect
        pts.append(((q[0] + px * np.cos(ang) - py * np.sin(ang)) * S,
                    (q[1] + px * np.sin(ang) + py * np.cos(ang)) * S))
    draw.polygon(pts, fill=to_px(color) + (255,))

# ------------------------------------------------------------------------------
# Oak
# ------------------------------------------------------------------------------

def make_oak(seed, crown_c, crown_r, trunk_top, lean):
    """crown_c: canopy center (px), crown_r: canopy radii (rx, ry, rz) in px."""
    rng = np.random.default_rng(seed)
    img, draw = new_canvas()
    fx, fy = FOOT

    ground_shadow(img, rng, width=crown_r[0] * 1.15)
    img = trunk(img, trunk_top, lambda t: 9.0 + 6.0 * t, lean, flare=1.0)
    draw = ImageDraw.Draw(img)

    # A few branches up into the crown
    for _ in range(6):
        x0, y0 = fx + lean * 4, trunk_top + rng.uniform(8, 30)
        ang = rng.uniform(-2.6, -0.5)
        L = rng.uniform(34, 56)
        draw.line([(x0 * S, y0 * S), ((x0 + np.cos(ang) * L) * S, (y0 + np.sin(ang) * L) * S)],
                  fill=to_px(BARK_DARK * 1.1) + (255,), width=int(5 * S))

    cx, cy = crown_c
    rx, ry, rz = crown_r

    # Deep shade inside the crown, so gaps between clumps show leaves in
    # shadow instead of the ground
    core = [(cx + rx * k * np.cos(a), cy + ry * k * np.sin(a))
            for a, k in zip(np.linspace(0, 2 * np.pi, 64, endpoint=False), rng.uniform(0.62, 0.74, 64))]
    draw.polygon([(x * S, y * S) for x, y in core], fill=to_px(OAK_RAMP[0] * 1.2) + (255,))

    # Clumps in the ellipsoid, biased outward so the crown is lumpy
    clumps = []
    for _ in range(80):
        d = rng.normal(0, 1, 3); d /= np.linalg.norm(d)
        r = rng.uniform(0.2, 0.85) ** 0.5
        p = np.array([cx + d[0] * rx * r, cy + d[1] * ry * r, d[2] * rz * r])
        clumps.append((p, rng.uniform(20, 32)))

    leaves = []
    for p, cr in clumps:
        for _ in range(int(cr * cr * 0.9)):
            n = rng.normal(0, 1, 3); n /= np.linalg.norm(n)
            if n[2] < -0.2: continue  # back of the clump is hidden anyway
            q = p + n * cr * rng.uniform(0.75, 1.0)
            e = ((q[0] - cx) / rx) ** 2 + ((q[1] - cy) / ry) ** 2 + (q[2] / rz) ** 2
            if e > rng.uniform(1.0, 1.12): continue  # inside the crown, a little ragged
            g = np.array([(q[0] - cx) / rx, (q[1] - cy) / ry, q[2] / rz])
            g /= np.linalg.norm(g) + 1e-9
            normal = 0.55 * n + 0.45 * g
            leaves.append((q, normal / np.linalg.norm(normal), g))

    leaves.sort(key=lambda l: l[0][2])  # back to front
    for q, normal, g in leaves:
        # leaves deep in the crown and on its underside are darker
        ao = np.clip(0.35 + 0.65 * (0.5 + 0.5 * g[2]), 0, 1) * np.clip(1.0 - 0.45 * max(0.0, g[1]), 0, 1)
        t = shade(normal, ao, rng)
        c = ramp(OAK_RAMP, t)
        if t > 0.8: c = c * 0.85 + OAK_TIP * 0.15  # warm sunlit tips
        leaf(draw, q, rng.uniform(2.6, 4.2), rng.uniform(0, np.pi), c)

    return finish(img)

# ------------------------------------------------------------------------------
# Pine
# ------------------------------------------------------------------------------

def make_pine(seed, top_y, base_y, base_r, tiers):
    """A cone of `tiers` drooping skirts from `top_y` (the tip) to `base_y`
    (the lowest skirt's hem), `base_r` px wide at the bottom."""
    rng = np.random.default_rng(seed)
    img, draw = new_canvas()
    fx, fy = FOOT

    ground_shadow(img, rng, width=base_r * 1.25)
    img = trunk(img, top_y + 40, lambda t: 5.0 + 7.0 * t, 0.0, flare=0.8)
    draw = ImageDraw.Draw(img)

    needles = []
    span = base_y - top_y
    for k in range(tiers):
        # Tier k hangs from its top (on the trunk) down and out to its hem.
        # Lower tiers are wider; each overlaps the next one down.
        f_top = k / tiers * 0.92
        f_hem = (k + 1.6) / tiers
        y_top = top_y + span * f_top
        y_hem = top_y + span * min(f_hem, 1.0)
        r_hem = base_r * min(f_hem, 1.0) ** 0.95 * rng.uniform(0.92, 1.05)
        count = int(260 + 1500 * min(f_hem, 1.0) ** 2)
        for _ in range(count):
            th = rng.uniform(0, 2 * np.pi)
            s = np.sqrt(rng.uniform(0.05, 1.0))  # 0 at the trunk, 1 at the hem
            r = r_hem * s * rng.uniform(0.85, 1.05)
            droop = (s ** 1.6) * (y_hem - y_top)
            q = np.array([fx + r * np.cos(th), y_top + droop * 0.55 + (y_hem - y_top) * 0.45 * s,
                          r * np.sin(th) * 0.9])
            # A skirt's surface leans out and up
            normal = np.array([np.cos(th), -0.55, np.sin(th)])
            normal /= np.linalg.norm(normal)
            # Shade of the tier above near the trunk, and the back of the tree
            ao = (0.45 + 0.55 * s) * np.clip(0.55 + 0.45 * np.sin(th), 0.2, 1.0)
            needles.append((q, normal, ao, th))

    needles.sort(key=lambda n: n[0][2])  # back to front
    for q, normal, ao, th in needles:
        t = shade(normal, ao, rng)
        c = ramp(PINE_RAMP, t)
        if t > 0.82: c = c * 0.85 + PINE_TIP * 0.15
        # A needle tuft: a short stroke hanging out and down from the trunk
        L = rng.uniform(4, 8)
        ang = np.pi / 2 - np.cos(th) * 1.1 + rng.normal(0, 0.3)
        x1, y1 = q[0] + np.cos(ang) * L, q[1] + np.sin(ang) * L
        draw.line([(q[0] * S, q[1] * S), (x1 * S, y1 * S)], fill=to_px(c) + (255,), width=int(2.4 * S))

    return finish(img)

# ------------------------------------------------------------------------------
# Main
# ------------------------------------------------------------------------------

# Two oaks (broad, and taller) and a pine. TreeVarieties in the game picks
# among them: oaks use the first two frames, pines the third.
FRAMES = [
    lambda: make_oak (seed=11, crown_c=(126, 170), crown_r=(110, 112, 84), trunk_top=200, lean=-0.4),
    lambda: make_oak (seed=23, crown_c=(130, 156), crown_r=(94, 124, 76),  trunk_top=196, lean=0.5),
    lambda: make_pine(seed=37, top_y=18, base_y=330, base_r=92, tiers=7),
]

parser = argparse.ArgumentParser()
parser.add_argument("--atlas", default="assets/graphics/atlas.png")
parser.add_argument("--at", type=int, nargs=2, default=(0, 1408), metavar=("X", "Y"),
                    help="top-left pixel of the first tree frame in the atlas")
parser.add_argument("--out", help="write the sprites alone here instead of into the atlas")
args = parser.parse_args()

sheet = Image.new("RGBA", (W * len(FRAMES), H), (0, 0, 0, 0))
for i, make in enumerate(FRAMES):
    sheet.paste(make(), (i * W, 0))

if args.out:
    sheet.save(args.out)
else:
    atlas = Image.open(args.atlas).convert("RGBA")
    x, y = args.at
    if atlas.width < x + sheet.width or atlas.height < y + sheet.height:
        grown = Image.new("RGBA", (max(atlas.width, x + sheet.width), max(atlas.height, y + sheet.height)), (0, 0, 0, 0))
        grown.paste(atlas, (0, 0))
        atlas = grown
    atlas.paste(sheet, (x, y))
    atlas.save(args.atlas, optimize=True)
