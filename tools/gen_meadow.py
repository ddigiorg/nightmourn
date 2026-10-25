# ==============================================================================
# tools/gen_meadow.py
#
# Generates the seamless 1024x1024 meadow ground texture (8x8 tiles of 128 px)
# in Age of Empires II: DE's light, warm green, and pastes it into the game
# atlas at its pixel box (AtlasPixels::Meadow in demos/01-player-armies).
#
# Tileable by construction: every noise layer is white noise filtered in the
# frequency domain (so it is periodic), and blades and flowers that cross an
# edge are drawn again on the other side. Deterministic (fixed seed).
#
#   python3 tools/gen_meadow.py                       # update assets/graphics/atlas.png
#   python3 tools/gen_meadow.py --out meadow.png      # texture only, for a look
#
# Needs numpy and Pillow.
# ==============================================================================

import argparse
import numpy as np
from PIL import Image

N = 1024
rng = np.random.default_rng(1999)

def band_noise(lo, hi, aniso=1.0):
    """Periodic noise with wavelengths between lo and hi pixels, unit std."""
    f = np.fft.fftfreq(N)
    fx, fy = np.meshgrid(f, f)
    r = np.sqrt(fx**2 + (fy * aniso)**2)
    lo_f, hi_f = 1.0 / hi, 1.0 / lo
    # smooth band: gaussian bump in log-frequency
    with np.errstate(divide="ignore"):
        lr = np.log(np.maximum(r, 1e-9))
    c = 0.5 * (np.log(lo_f) + np.log(hi_f))
    w = 0.5 * (np.log(hi_f) - np.log(lo_f)) + 1e-6
    filt = np.exp(-0.5 * ((lr - c) / w) ** 2)
    filt[0, 0] = 0.0
    spec = np.fft.fft2(rng.standard_normal((N, N))) * filt
    out = np.real(np.fft.ifft2(spec))
    return (out - out.mean()) / out.std()

large  = band_noise(256, 1024)       # broad light/shade drifts
mid    = band_noise(48, 160)         # grass patches
small  = band_noise(10, 28)          # clumps
fine   = band_noise(3, 7)            # blade texture
streak = band_noise(3, 9, aniso=0.35) # slightly stretched strokes

# Brightness and warmth fields
light = 0.55 * large + 0.45 * mid + 0.30 * small
warm  = 0.6 * band_noise(128, 512) + 0.4 * mid

def hexc(h):
    return np.array([int(h[i:i+2], 16) for i in (1, 3, 5)], dtype=np.float64)

# AoE2 DE-style meadow: bright, slightly warm yellow-green
deep   = hexc("#7a9b34")
base   = hexc("#97b545")
bright = hexc("#b3cd5e")
warmc  = hexc("#b1c255")   # yellower highlights
coolc  = hexc("#86ad48")   # bluer-green patches

t = np.clip(0.5 + 0.22 * light, 0.0, 1.0)[..., None]
col = np.where(t < 0.5, deep + (base - deep) * (t / 0.5), base + (bright - base) * ((t - 0.5) / 0.5))
wt = np.clip(0.5 + 0.25 * warm, 0.0, 1.0)[..., None]
col = col * 0.8 + (coolc * (1 - wt) + warmc * wt) * 0.2

# Blade texture: fine noise darkens/lightens a little
blade = (0.55 * fine + 0.45 * streak)[..., None]
col = col * (1.0 + 0.07 * blade)

# Darker clumps where small noise is high (tufts)
tuft = np.clip((small - 1.2) * 0.6, 0.0, 1.0)[..., None]
col = col * (1.0 - 0.12 * tuft)

img = np.clip(col, 0, 255)

# Grass blades: many short strokes, each a lighter or darker shade of the
# ground under it, drawn 2x supersampled with wrap-around, then averaged down
from PIL import ImageDraw
S = 2
canvas = Image.fromarray(np.kron(img, np.ones((S, S, 1))).astype(np.uint8), "RGB")
draw = ImageDraw.Draw(canvas)
def blades(count, length, width, shade_lo, shade_hi, cluster=False):
    centers = rng.uniform(0, N, (count, 2))
    for cx, cy in centers:
        n = rng.integers(4, 9) if cluster else 1
        base_ang = rng.uniform(0, 2 * np.pi)
        for _ in range(n):
            x0 = cx + (rng.normal(0, 3) if cluster else 0)
            y0 = cy + (rng.normal(0, 3) if cluster else 0)
            ang = base_ang + rng.normal(0, 0.6 if cluster else 1.5)
            L = rng.uniform(*length)
            x1, y1 = x0 + np.cos(ang) * L, y0 + np.sin(ang) * L
            c = img[int(y0) % N, int(x0) % N] * rng.uniform(shade_lo, shade_hi)
            c = tuple(int(v) for v in np.clip(c, 0, 255))
            w = max(1, int(round(rng.uniform(*width) * S)))
            for ox in (-N, 0, N):
                for oy in (-N, 0, N):
                    ax, ay, bx, by = x0 + ox, y0 + oy, x1 + ox, y1 + oy
                    if max(ax, bx) < -4 or min(ax, bx) > N + 4 or max(ay, by) < -4 or min(ay, by) > N + 4:
                        continue
                    draw.line([(ax * S, ay * S), (bx * S, by * S)], fill=c, width=w)
blades(22000, (3, 7),  (1.0, 1.6), 0.86, 0.96)          # shadowed blades
blades(22000, (3, 7),  (1.0, 1.6), 1.05, 1.16)          # lit blades
blades(900,   (5, 10), (1.2, 1.8), 0.80, 0.90, True)    # darker tufts
blades(700,   (4, 9),  (1.0, 1.5), 1.10, 1.22, True)    # bright tufts
img = np.asarray(canvas.resize((N, N), Image.BOX)).astype(np.float64)

def stamp(img, cx, cy, r, color, alpha):
    ys = np.arange(int(cy - r - 1), int(cy + r + 2))
    xs = np.arange(int(cx - r - 1), int(cx + r + 2))
    for y in ys:
        for x in xs:
            d = np.hypot(x - cx, y - cy)
            a = alpha * np.clip(r + 0.5 - d, 0.0, 1.0)
            if a <= 0: continue
            img[y % N, x % N] = img[y % N, x % N] * (1 - a) + color * a

# Sparse tiny flowers (pale yellow, white), in loose clusters
flower_cols = [hexc("#f2ec9a"), hexc("#f6f3df"), hexc("#e9d36a")]
for _ in range(70):
    cx, cy = rng.uniform(0, N, 2)
    for _ in range(rng.integers(2, 7)):
        x, y = cx + rng.normal(0, 14), cy + rng.normal(0, 14)
        stamp(img, x, y, rng.uniform(1.2, 2.0), flower_cols[rng.integers(0, 3)], 0.85)

# A few dry/earth specks
for _ in range(120):
    x, y = rng.uniform(0, N, 2)
    stamp(img, x, y, rng.uniform(1.0, 1.8), hexc("#8c8a45"), 0.5)

meadow = Image.fromarray(img.astype(np.uint8), "RGB").convert("RGBA")

parser = argparse.ArgumentParser()
parser.add_argument("--atlas", default="assets/graphics/atlas.png")
parser.add_argument("--at", type=int, nargs=2, default=(0, 384), metavar=("X", "Y"),
                    help="top-left pixel of the meadow in the atlas")
parser.add_argument("--out", help="write the texture alone here instead of into the atlas")
args = parser.parse_args()

if args.out:
    meadow.save(args.out)
else:
    atlas = Image.open(args.atlas).convert("RGBA")
    x, y = args.at
    if atlas.width < x + N or atlas.height < y + N:
        grown = Image.new("RGBA", (max(atlas.width, x + N), max(atlas.height, y + N)), (0, 0, 0, 0))
        grown.paste(atlas, (0, 0))
        atlas = grown
    atlas.paste(meadow, (x, y))
    atlas.save(args.atlas, optimize=True)
