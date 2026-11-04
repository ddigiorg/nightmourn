# ==============================================================================
# tools/gen_buildings.py
#
# Generates the placeholder building sprites for the 2:1 isometric view, in
# the style of the unit figures (gen_units.py): masks whose white takes the
# owner's color (Sprite::tint), shaded grays that keep the faces apart once
# tinted, black outlines, and the building's name in black. Buildings are
# boxes standing on their footprint, the way Age of Empires II's buildings
# fill theirs:
#
#   data/base/graphics/entity/building/<name>_mask.png
#   data/base/graphics/gui/item/<name>.png              72 x 72 item icons
#
# Iso art is 128 px per tile diamond width (SPRITE_PIXELS_PER_TILE): a tile
# is a 128 x 64 px diamond, and a 1 upright tile height is 128 px. Each
# picture's pivot is its footprint's center, which is where the game draws
# it; the pivots are printed for data/base/configs/entity/buildings.json.
#
# The wall sheet has a frame per set of connected sides, the frame index
# being the side bits (north/+y 1, east/+x 2, south/-y 4, west/-x 8, as
# Building::connections): a post with an arm out to each connected side, so
# a row of walls reads as one wall.
#
#   python3 tools/gen_buildings.py                 # write into data/base/graphics
#   python3 tools/gen_buildings.py --out somewhere # somewhere else, for a look
#
# Needs Pillow. Deterministic.
# ==============================================================================

import argparse
import os
from PIL import Image, ImageDraw, ImageFont

TILE    = 128  # px per tile diamond width; the diamond is TILE x TILE / 2
HALF    = TILE // 2
SS      = 2    # supersampling
OUTLINE = 4    # black edge, px
PAD     = 6
BLACK   = (0, 0, 0, 255)
CLEAR   = (0, 0, 0, 0)

# Face shades: the tint multiplies them, so the faces stay apart in any color
TOP, LEFT, RIGHT = 255, 214, 160

class Canvas:
    """An image of a footprint `n` x `m` tiles with room for `height` px
    above it. World (x, y, z) in tiles (z in px) maps to image px with the
    footprint's min corner at the bottom."""
    def __init__(self, n, m, height):
        self.w  = HALF * (n + m) + 2 * PAD
        self.h  = HALF // 2 * (n + m) + height + 2 * PAD
        self.ox = HALF * m + PAD
        self.oy = self.h - PAD
        self.n, self.m = n, m
        self.img  = Image.new("RGBA", (self.w * SS, self.h * SS), CLEAR)
        self.draw = ImageDraw.Draw(self.img)

    def px(self, x, y, z=0.0):
        return ((self.ox + HALF * (x - y)) * SS, (self.oy - HALF / 2 * (x + y) - z) * SS)

    def pivot(self):
        x, y = self.px(self.n / 2, self.m / 2)
        return [round(x / SS), round(y / SS)]

    def face(self, pts, shade):
        poly = [self.px(*p) for p in pts]
        self.draw.polygon(poly, fill=(shade, shade, shade, 255))
        self.draw.line(poly + [poly[0]], fill=BLACK, width=OUTLINE * SS, joint="curve")

    def box(self, x0, y0, x1, y1, h, z0=0):
        """A box from (x0, y0) to (x1, y1), from `z0` up to `h` px: its two
        front faces and its top."""
        self.face([(x0, y0, z0), (x0, y1, z0), (x0, y1, h), (x0, y0, h)], LEFT)   # faces -x
        self.face([(x0, y0, z0), (x1, y0, z0), (x1, y0, h), (x0, y0, h)], RIGHT)  # faces -y
        self.face([(x0, y0, h), (x1, y0, h), (x1, y1, h), (x0, y1, h)], TOP)

    def boxes(self, boxes):
        """Several boxes, back to front."""
        for b in sorted(boxes, key=lambda b: -(b[0] + b[1] + b[2] + b[3])):
            self.box(*b)

    def label(self, text, at, size):
        font = ImageFont.load_default(size=size * SS)
        x, y = self.px(*at)
        self.draw.text((x, y), text, fill=BLACK, font=font, anchor="mm")

    def image(self):
        return self.img.resize((self.w, self.h), Image.LANCZOS)

# ------------------------------------------------------------------------------
# Buildings
# ------------------------------------------------------------------------------

def gen_cube(name, n, height, font_size):
    """A box filling its n x n footprint, its name across the front corner."""
    c = Canvas(n, n, height)
    inset = 0.06
    c.box(inset, inset, n - inset, n - inset, height)
    c.label(name, (0.06, 0.06, height * 0.5), font_size)
    return c.image(), c.pivot()

def gen_banner():
    """A pole 2 tiles tall on a small plinth, a flag hanging from it."""
    c = Canvas(1, 1, 270)
    c.box(0.3, 0.3, 0.7, 0.7, 10)                    # plinth
    c.box(0.46, 0.46, 0.54, 0.54, 256)               # pole
    # The flag hangs off the pole toward the viewer's left: a flat sheet
    # in the x = 0.5 plane, swallowtail at the bottom
    top, bottom = 250, 120
    c.face([(0.45, 0.5, top), (0.45, -0.25, top), (0.45, -0.25, bottom),
            (0.45, 0.125, bottom + 22), (0.45, 0.5, bottom)], TOP)
    c.label("banner", (0.45, 0.12, (top + bottom) / 2 + 20), 13)
    return c.image(), c.pivot()

def gen_torch():
    """A post on a plinth with a lit head: the light sits on top
    (glow_height of the "torch" light, 1.15 upright tiles)."""
    c = Canvas(1, 1, 160)
    c.box(0.25, 0.25, 0.75, 0.75, 14)                # plinth
    c.box(0.45, 0.45, 0.55, 0.55, 124)               # post
    c.box(0.37, 0.37, 0.63, 0.63, 150, 120)          # head, where the flame is
    c.label("torch", (0.25, 0.25, 30), 12)
    return c.image(), c.pivot()

def gen_wall_frame(bits, height):
    c = Canvas(1, 1, height)
    a, b = 0.24, 0.76 # post and arm width
    boxes = [(a, a, b, b, height)]
    if bits & 1: boxes.append((a, b, b, 1.0, height))   # north, +y
    if bits & 2: boxes.append((b, a, 1.0, b, height))   # east, +x
    if bits & 4: boxes.append((a, 0.0, b, a, height))   # south, -y
    if bits & 8: boxes.append((0.0, a, a, b, height))   # west, -x
    c.boxes(boxes)
    c.label("wall", (a, a, height * 0.5), 12)
    return c.image(), c.pivot()

def gen_wall():
    frames = [gen_wall_frame(bits, 56) for bits in range(16)]
    w, h = frames[0][0].size
    sheet = Image.new("RGBA", (w * 16, h), CLEAR)
    for bits, (img, _) in enumerate(frames):
        sheet.paste(img, (bits * w, 0))
    return sheet, frames[0][1], (w, h)

# ------------------------------------------------------------------------------
# Item icons: the building's mask, tinted the player's blue, on a dark tile
# like the other item icons (gen_ui.py)
# ------------------------------------------------------------------------------

ICON      = 72
ICON_TINT = (110, 160, 235)

def gen_icon(mask):
    bg = Image.new("RGBA", (ICON, ICON), (20, 26, 36, 255))
    d = ImageDraw.Draw(bg)
    for i in range(ICON // 2):
        a = int(18 * (1 - i / (ICON / 2)))
        d.rectangle([i, i, ICON - 1 - i, ICON - 1 - i], outline=(20 + a, 26 + a, 36 + a, 255))
    m = mask.copy()
    m.thumbnail((ICON - 12, ICON - 12), Image.LANCZOS)
    r, g, b, a = m.split()
    tinted = Image.merge("RGBA", (
        r.point(lambda v: v * ICON_TINT[0] // 255),
        g.point(lambda v: v * ICON_TINT[1] // 255),
        b.point(lambda v: v * ICON_TINT[2] // 255),
        a))
    bg.alpha_composite(tinted, ((ICON - m.width) // 2, (ICON - m.height) // 2))
    # Bevel, lighter top-left and darker bottom-right
    d = ImageDraw.Draw(bg)
    d.line([(0, ICON - 1), (0, 0), (ICON - 1, 0)], fill=(90, 100, 115, 255), width=2)
    d.line([(1, ICON - 1), (ICON - 1, ICON - 1), (ICON - 1, 1)], fill=(8, 10, 14, 255), width=2)
    return bg

# ------------------------------------------------------------------------------
# Main
# ------------------------------------------------------------------------------

def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "base", "graphics")
    ap = argparse.ArgumentParser(description="Generate the placeholder building masks and item icons")
    ap.add_argument("--out", default=root, help="graphics folder to write entity/building and gui/item into")
    args = ap.parse_args()

    wall, wall_pivot, wall_frame = gen_wall()
    pictures = {
        "banner":   gen_banner(),
        "torch":    gen_torch(),
        "house":    gen_cube("house",    2, 110, 20),
        "crafter":  gen_cube("crafter",  3, 120, 24),
        "barracks": gen_cube("barracks", 3, 150, 24),
    }

    bdir = os.path.join(args.out, "entity", "building")
    idir = os.path.join(args.out, "gui", "item")
    os.makedirs(bdir, exist_ok=True)
    os.makedirs(idir, exist_ok=True)

    path = os.path.join(bdir, "wall_mask.png")
    wall.save(path, optimize=True)
    print(f"wrote {os.path.normpath(path)}  frame {wall_frame[0]} x {wall_frame[1]}  pivot {wall_pivot}")
    for name, (img, pivot) in pictures.items():
        path = os.path.join(bdir, f"{name}_mask.png")
        img.save(path, optimize=True)
        print(f"wrote {os.path.normpath(path)}  {img.width} x {img.height}  pivot {pivot}")

    # The wall's icon is a straight east-west piece
    icons = {name: img for name, (img, _) in pictures.items()}
    icons["wall"] = wall.crop((10 * wall_frame[0], 0, 11 * wall_frame[0], wall_frame[1]))
    for name, img in icons.items():
        path = os.path.join(idir, f"{name}.png")
        gen_icon(img).save(path, optimize=True)
        print("wrote", os.path.normpath(path))

if __name__ == "__main__":
    main()
