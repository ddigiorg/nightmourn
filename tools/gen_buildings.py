# ==============================================================================
# tools/gen_buildings.py
#
# Generates the placeholder building sprites for the top-down view, in the
# style of the unit figures (gen_units.py): masks whose white takes the
# owner's color (Sprite::tint), a bold black outline, and the building's
# name in black, centered. Buildings are squares filling their footprint,
# seen from above:
#
#   data/base/graphics/entity/building/<name>_mask.png
#   data/base/graphics/gui/item/<name>.png              72 x 72 item icons
#
# Art is 128 px per tile (SPRITE_PIXELS_PER_TILE), so a house's 2 x 2 tile
# footprint is 256 px square. Each picture's pivot is the footprint's
# center (the middle of a square; the foot of the banner's and torch's
# pole), which is where the game draws it; the pivots are printed for
# data/base/configs/entity/buildings.json.
#
# The wall and gate sheets have a frame per set of connected sides, the
# frame index being the side bits (north 1, east 2, south 4, west 8, as
# Building::connections): a post with an arm out to each connected side,
# so a row of walls reads as one wall. A gate is the same with a planked
# door across its middle.
#
#   python3 tools/gen_buildings.py                 # write into data/base/graphics
#   python3 tools/gen_buildings.py --out somewhere # somewhere else, for a look
#
# Needs Pillow. Deterministic.
# ==============================================================================

import argparse
import os
from PIL import Image, ImageDraw, ImageFont

TILE   = 128 # px per tile
BORDER = 8   # black edge around the white fill, as the figures'
BLACK  = (0, 0, 0, 255)
WHITE  = (255, 255, 255, 255)
CLEAR  = (0, 0, 0, 0)

def outlined_box(draw, box):
    """A white box with a black BORDER inside its edge."""
    x0, y0, x1, y1 = box
    draw.rectangle([x0, y0, x1 - 1, y1 - 1], fill=BLACK)
    draw.rectangle([x0 + BORDER, y0 + BORDER, x1 - 1 - BORDER, y1 - 1 - BORDER], fill=WHITE)

def label(draw, center, text, size):
    font = ImageFont.load_default(size=size)
    draw.text(center, text, fill=BLACK, font=font, anchor="mm")

# ------------------------------------------------------------------------------
# Buildings
# ------------------------------------------------------------------------------

def gen_box(name, tiles, font_size):
    """A square filling an n x n footprint, its name in the middle."""
    w = h = tiles * TILE
    img = Image.new("RGBA", (w, h), CLEAR)
    d = ImageDraw.Draw(img)
    outlined_box(d, (0, 0, w, h))
    # An inner line, a hint of a roof edge, so it doesn't read as a hole
    inset = 24 * tiles // 2
    d.rectangle([inset, inset, w - 1 - inset, h - 1 - inset], outline=BLACK, width=4)
    label(d, (w / 2, h / 2), name, font_size)
    return img, [w // 2, h // 2]

def gen_banner():
    """A short pole (1.5 tiles) with the full-size flag hanging from its
    top: the pole's foot is the pivot."""
    w, h = 64, 192
    img = Image.new("RGBA", (w, h), CLEAR)
    d = ImageDraw.Draw(img)
    # Pole: white with a black edge, full height
    d.rectangle([24, 0, 39, h - 1], fill=BLACK)
    d.rectangle([28, 4, 35, h - 5], fill=WHITE)
    # Flag hanging from the top, a swallowtail at its foot, the word down it
    flag = [(0, 8), (w - 1, 8), (w - 1, 172), (w / 2, 148), (0, 172)]
    d.polygon(flag, fill=BLACK)
    inner = [(BORDER, 8 + BORDER), (w - 1 - BORDER, 8 + BORDER), (w - 1 - BORDER, 158),
             (w / 2, 138), (BORDER, 158)]
    d.polygon(inner, fill=WHITE)
    font = ImageFont.load_default(size=20)
    for i, ch in enumerate("BANNER"):
        d.text((w / 2, 30 + i * 20), ch, fill=BLACK, font=font, anchor="mm")
    return img, [w // 2, h - 6]

def gen_torch():
    """A post on a plinth with a square head, under a tile tall: the post's
    foot is the pivot, and the "building-torch" light glows at the head
    (glow_height 0.6 tiles, 76 px above the foot)."""
    w, h = 64, 112
    img = Image.new("RGBA", (w, h), CLEAR)
    d = ImageDraw.Draw(img)
    d.rectangle([24, 30, 39, h - 11], fill=BLACK)   # post, edged like the banner's pole
    d.rectangle([28, 30, 35, h - 11], fill=WHITE)
    outlined_box(d, (10, h - 26, w - 10, h))   # plinth
    outlined_box(d, (12, 4, w - 12, 44))       # head, where the flame is
    return img, [w // 2, h - 12]

def gen_connector_frame(bits, door):
    """One wall tile: a post, with an arm to each side in `bits`. A gate
    (`door`) gets planks across its middle."""
    s = TILE
    POST = 20 # inset of the post and arms from the tile edge
    img = Image.new("RGBA", (s, s), CLEAR)
    d = ImageDraw.Draw(img)
    # Image y runs down, so north (+y in the world) is the top
    boxes = [(POST, POST, s - POST, s - POST)]
    if bits & 1: boxes.append((POST, 0, s - POST, s - POST))   # north
    if bits & 2: boxes.append((POST, POST, s, s - POST))       # east
    if bits & 4: boxes.append((POST, POST, s - POST, s))       # south
    if bits & 8: boxes.append((0, POST, s - POST, s - POST))   # west
    for b in boxes: d.rectangle([b[0], b[1], b[2] - 1, b[3] - 1], fill=BLACK)
    for b in boxes:
        # Shrink by the border, except where the box runs off the tile edge
        x0 = b[0] + (BORDER if b[0] > 0 else 0)
        y0 = b[1] + (BORDER if b[1] > 0 else 0)
        x1 = b[2] - (BORDER if b[2] < s else 0)
        y1 = b[3] - (BORDER if b[3] < s else 0)
        d.rectangle([x0, y0, x1 - 1, y1 - 1], fill=WHITE)
    if door:
        # Planks across the way through: lines along the wall's run (east-
        # west unless it only joins north or south)
        along_y = bool(bits & 5) and not (bits & 10)
        for i in range(1, 4):
            t = POST + BORDER + i * (s - 2 * POST - 2 * BORDER) // 4
            if along_y: d.line([(t, POST), (t, s - POST)], fill=BLACK, width=4)
            else:       d.line([(POST, t), (s - POST, t)], fill=BLACK, width=4)
        d.rectangle([POST, POST, s - POST - 1, s - POST - 1], outline=BLACK, width=BORDER)
    font = ImageFont.load_default(size=22)
    d.text((s / 2, s / 2), "gate" if door else "wall", fill=BLACK, font=font, anchor="mm",
           stroke_width=3 if door else 0, stroke_fill=WHITE)
    return img

def gen_connector(door):
    sheet = Image.new("RGBA", (TILE * 16, TILE), CLEAR)
    for bits in range(16):
        sheet.paste(gen_connector_frame(bits, door), (bits * TILE, 0))
    return sheet, [TILE // 2, TILE // 2]

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

    pictures = {
        "banner":   gen_banner(),
        "wall":     gen_connector(door=False),
        "gate":     gen_connector(door=True),
        "torch":    gen_torch(),
        "house":    gen_box("house",    2, 44),
        "crafter":  gen_box("crafter",  3, 56),
        "barracks": gen_box("barracks", 3, 56),
    }

    bdir = os.path.join(args.out, "entity", "building")
    idir = os.path.join(args.out, "gui", "item")
    os.makedirs(bdir, exist_ok=True)
    os.makedirs(idir, exist_ok=True)
    for name, (img, pivot) in pictures.items():
        path = os.path.join(bdir, f"{name}_mask.png")
        img.save(path, optimize=True)
        print(f"wrote {os.path.normpath(path)}  {img.width} x {img.height}  pivot {pivot}")

    # Wall and gate icons are a straight east-west piece
    icons = {name: img for name, (img, _) in pictures.items()}
    for name in ("wall", "gate"):
        icons[name] = icons[name].crop((10 * TILE, 0, 11 * TILE, TILE))
    for name, img in icons.items():
        path = os.path.join(idir, f"{name}.png")
        gen_icon(img).save(path, optimize=True)
        print("wrote", os.path.normpath(path))

if __name__ == "__main__":
    main()
